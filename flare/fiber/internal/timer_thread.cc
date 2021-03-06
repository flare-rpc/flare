// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// fiber - A M:N threading library to make applications more concurrent.


#include <queue>                           // heap functions
#include "flare/base/scoped_lock.h"
#include "flare/log/logging.h"
#include "flare/hash/murmurhash3.h"   // fmix64
#include "flare/memory/resource_pool.h"
#include "flare/variable/all.h"
#include "flare/fiber/internal/sys_futex.h"
#include "flare/fiber/internal/timer_thread.h"
#include "flare/fiber/internal/log.h"

namespace flare::fiber_internal {

// Defined in task_control.cpp
void run_worker_startfn();

const TimerThread::TaskId TimerThread::INVALID_TASK_ID = 0;

TimerThreadOptions::TimerThreadOptions()
    : num_buckets(13) {
}

// A task contains the necessary information for running fn(arg).
// Tasks are created in Bucket::schedule and destroyed in TimerThread::run
struct FLARE_CACHELINE_ALIGNMENT TimerThread::Task {
    Task* next;                 // For linking tasks in a Bucket.
    int64_t run_time;           // run the task at this realtime
    void (*fn)(void*);          // the fn(arg) to run
    void* arg;
    // Current TaskId, checked against version in TimerThread::run to test
    // if this task is unscheduled.
    TaskId task_id;
    // initial_version:     not run yet
    // initial_version + 1: running
    // initial_version + 2: removed (also the version of next Task reused
    //                      this struct)
    std::atomic<uint32_t> version;

    Task() : version(2/*skip 0*/) {}

    // Run this task and delete this struct.
    // Returns true if fn(arg) did run.
    bool run_and_delete();

    // Delete this struct if this task was unscheduled.
    // Returns true on deletion.
    bool try_delete();
};

// Timer tasks are sharded into different Buckets to reduce contentions.
class FLARE_CACHELINE_ALIGNMENT TimerThread::Bucket {
public:
    Bucket()
        : _nearest_run_time(std::numeric_limits<int64_t>::max())
        , _task_head(NULL) {
    }

    ~Bucket() {}

    struct ScheduleResult {
        TimerThread::TaskId task_id;
        bool earlier;
    };
    
    // Schedule a task into this bucket.
    // Returns the TaskId and if it has the nearest run time.
    ScheduleResult schedule(void (*fn)(void*), void* arg,
                            const timespec& abstime);

    // Pull all scheduled tasks.
    // This function is called in timer thread.
    Task* consume_tasks();

private:
    internal::FastPthreadMutex _mutex;
    int64_t _nearest_run_time;
    Task* _task_head;
};

// Utilies for making and extracting TaskId.
inline TimerThread::TaskId make_task_id(
    flare::ResourceId<TimerThread::Task> slot, uint32_t version) {
    return TimerThread::TaskId((((uint64_t)version) << 32) | slot.value);
}

inline
flare::ResourceId<TimerThread::Task> slot_of_task_id(TimerThread::TaskId id) {
    flare::ResourceId<TimerThread::Task> slot = { (id & 0xFFFFFFFFul) };
    return slot;
}

inline uint32_t version_of_task_id(TimerThread::TaskId id) {
    return (uint32_t)(id >> 32);
}

inline bool task_greater(const TimerThread::Task* a, const TimerThread::Task* b) {
    return a->run_time > b->run_time;
}

void* TimerThread::run_this(void* arg) {
    static_cast<TimerThread*>(arg)->run();
    return NULL;
}

TimerThread::TimerThread()
    : _started(false)
    , _stop(false)
    , _buckets(NULL)
    , _nearest_run_time(std::numeric_limits<int64_t>::max())
    , _nsignals(0)
    , _thread(0) {
}

TimerThread::~TimerThread() {
    stop_and_join();
    delete [] _buckets;
    _buckets = NULL;
}

int TimerThread::start(const TimerThreadOptions* options_in) {
    if (_started) {
        return 0;
    }
    if (options_in) {
        _options = *options_in;
    }
    if (_options.num_buckets == 0) {
        FLARE_LOG(ERROR) << "num_buckets can't be 0";
        return EINVAL;
    }
    if (_options.num_buckets > 1024) {
        FLARE_LOG(ERROR) << "num_buckets=" << _options.num_buckets << " is too big";
        return EINVAL;
    }
    _buckets = new (std::nothrow) Bucket[_options.num_buckets];
    if (NULL == _buckets) {
        FLARE_LOG(ERROR) << "Fail to new _buckets";
        return ENOMEM;
    }        
    const int ret = pthread_create(&_thread, NULL, TimerThread::run_this, this);
    if (ret) {
        return ret;
    }
    _started = true;
    return 0;
}

TimerThread::Task* TimerThread::Bucket::consume_tasks() {
    Task* head = NULL;
    if (_task_head) { // NOTE: schedule() and consume_tasks() are sequenced
        // by TimerThread._nearest_run_time and fenced by TimerThread._mutex.
        // We can avoid touching the mutex and related cacheline when the
        // bucket is actually empty.
        FLARE_SCOPED_LOCK(_mutex);
        if (_task_head) {
            head = _task_head;
            _task_head = NULL;
            _nearest_run_time = std::numeric_limits<int64_t>::max();
        }
    }
    return head;
}

TimerThread::Bucket::ScheduleResult
TimerThread::Bucket::schedule(void (*fn)(void*), void* arg,
                              const timespec& abstime) {
    flare::ResourceId<Task> slot_id;
    Task* task = flare::get_resource<Task>(&slot_id);
    if (task == NULL) {
        ScheduleResult result = { INVALID_TASK_ID, false };
        return result;
    }
    task->next = NULL;
    task->fn = fn;
    task->arg = arg;
    task->run_time = flare::time_point::from_timespec(abstime).to_unix_micros();
    uint32_t version = task->version.load(std::memory_order_relaxed);
    if (version == 0) {  // skip 0.
        task->version.fetch_add(2, std::memory_order_relaxed);
        version = 2;
    }
    const TaskId id = make_task_id(slot_id, version);
    task->task_id = id;
    bool earlier = false;
    {
        FLARE_SCOPED_LOCK(_mutex);
        task->next = _task_head;
        _task_head = task;
        if (task->run_time < _nearest_run_time) {
            _nearest_run_time = task->run_time;
            earlier = true;
        }
    }
    ScheduleResult result = { id, earlier };
    return result;
}

TimerThread::TaskId TimerThread::schedule(
    void (*fn)(void*), void* arg, const timespec& abstime) {
    if (_stop.load(std::memory_order_relaxed) || !_started) {
        // Not add tasks when TimerThread is about to stop.
        return INVALID_TASK_ID;
    }
    // Hashing by pthread id is better for cache locality.
    const Bucket::ScheduleResult result = 
        _buckets[flare::hash::fmix64(pthread_numeric_id()) % _options.num_buckets]
        .schedule(fn, arg, abstime);
    if (result.earlier) {
        bool earlier = false;
        const int64_t run_time =  flare::time_point::from_timespec(abstime).to_unix_micros();
        {
            FLARE_SCOPED_LOCK(_mutex);
            if (run_time < _nearest_run_time) {
                _nearest_run_time = run_time;
                ++_nsignals;
                earlier = true;
            }
        }
        if (earlier) {
            futex_wake_private(&_nsignals, 1);
        }
    }
    return result.task_id;
}

// Notice that we don't recycle the Task in this function, let TimerThread::run
// do it. The side effect is that we may allocated many unscheduled tasks before
// TimerThread wakes up. The number is approximately qps * timeout_s. Under the
// precondition that ResourcePool<Task> caches 128K for each thread, with some
// further calculations, we can conclude that in a RPC scenario:
//   when timeout / latency < 2730 (128K / sizeof(Task))
// unscheduled tasks do not occupy additional memory. 2730 is a large ratio
// between timeout and latency in most RPC scenarios, this is why we don't
// try to reuse tasks right now inside unschedule() with more complicated code.
int TimerThread::unschedule(TaskId task_id) {
    const flare::ResourceId<Task> slot_id = slot_of_task_id(task_id);
    Task* const task = flare::address_resource(slot_id);
    if (task == NULL) {
        FLARE_LOG(ERROR) << "Invalid task_id=" << task_id;
        return -1;
    }
    const uint32_t id_version = version_of_task_id(task_id);
    uint32_t expected_version = id_version;
    // This CAS is rarely contended, should be fast.
    // The acquire fence is paired with release fence in Task::run_and_delete
    // to make sure that we see all changes brought by fn(arg).
    if (task->version.compare_exchange_strong(
            expected_version, id_version + 2,
            std::memory_order_acquire)) {
        return 0;
    }
    return (expected_version == id_version + 1) ? 1 : -1;
}

bool TimerThread::Task::run_and_delete() {
    const uint32_t id_version = version_of_task_id(task_id);
    uint32_t expected_version = id_version;
    // This CAS is rarely contended, should be fast.
    if (version.compare_exchange_strong(
            expected_version, id_version + 1, std::memory_order_relaxed)) {
        fn(arg);
        // The release fence is paired with acquire fence in
        // TimerThread::unschedule to make changes of fn(arg) visible.
        version.store(id_version + 2, std::memory_order_release);
        flare::return_resource(slot_of_task_id(task_id));
        return true;
    } else if (expected_version == id_version + 2) {
        // already unscheduled.
        flare::return_resource(slot_of_task_id(task_id));
        return false;
    } else {
        // Impossible.
        FLARE_LOG(ERROR) << "Invalid version=" << expected_version
                   << ", expecting " << id_version + 2;
        return false;
    }
}

bool TimerThread::Task::try_delete() {
    const uint32_t id_version = version_of_task_id(task_id);
    if (version.load(std::memory_order_relaxed) != id_version) {
        FLARE_CHECK_EQ(version.load(std::memory_order_relaxed), id_version + 2);
        flare::return_resource(slot_of_task_id(task_id));
        return true;
    }
    return false;
}

template <typename T>
static T deref_value(void* arg) {
    return *(T*)arg;
}

void TimerThread::run() {
    run_worker_startfn();
    int64_t last_sleep_time = flare::get_current_time_micros();
    BT_VLOG << "Started TimerThread=" << pthread_self();

    // min heap of tasks (ordered by run_time)
    std::vector<Task*> tasks;
    tasks.reserve(4096);

    // vars
    size_t nscheduled = 0;
    flare::variable::PassiveStatus<size_t> nscheduled_var(deref_value<size_t>, &nscheduled);
    flare::variable::PerSecond<flare::variable::PassiveStatus<size_t> > nscheduled_second(&nscheduled_var);
    size_t ntriggered = 0;
    flare::variable::PassiveStatus<size_t> ntriggered_var(deref_value<size_t>, &ntriggered);
    flare::variable::PerSecond<flare::variable::PassiveStatus<size_t> > ntriggered_second(&ntriggered_var);
    double busy_seconds = 0;
    flare::variable::PassiveStatus<double> busy_seconds_var(deref_value<double>, &busy_seconds);
    flare::variable::PerSecond<flare::variable::PassiveStatus<double> > busy_seconds_second(&busy_seconds_var);
    if (!_options.variable_prefix.empty()) {
        nscheduled_second.expose_as(_options.variable_prefix, "scheduled_second");
        ntriggered_second.expose_as(_options.variable_prefix, "triggered_second");
        busy_seconds_second.expose_as(_options.variable_prefix, "usage");
    }
    
    while (!_stop.load(std::memory_order_relaxed)) {
        // Clear _nearest_run_time before consuming tasks from buckets.
        // This helps us to be aware of earliest task of the new tasks before we
        // would run the consumed tasks.
        {
            FLARE_SCOPED_LOCK(_mutex);
            _nearest_run_time = std::numeric_limits<int64_t>::max();
        }
        
        // Pull tasks from buckets.
        for (size_t i = 0; i < _options.num_buckets; ++i) {
            Bucket& bucket = _buckets[i];
            for (Task* p = bucket.consume_tasks(); p != nullptr; ++nscheduled) {
                // p->next should be kept first
                // in case of the deletion of Task p which is unscheduled
                Task* next_task = p->next;

                if (!p->try_delete()) { // remove the task if it's unscheduled
                    tasks.push_back(p);
                    std::push_heap(tasks.begin(), tasks.end(), task_greater);
                }
                p = next_task;
            }
        }

        bool pull_again = false;
        while (!tasks.empty()) {
            Task* task1 = tasks[0];  // the about-to-run task
            if (flare::get_current_time_micros() < task1->run_time) {  // not ready yet.
                break;
            }
            // Each time before we run the earliest task (that we think), 
            // check the globally shared _nearest_run_time. If a task earlier
            // than task1 was scheduled during pulling from buckets, we'll
            // know. In RPC scenarios, _nearest_run_time is not often changed by
            // threads because the task needs to be the earliest in its bucket,
            // since run_time of scheduled tasks are often in ascending order,
            // most tasks are unlikely to be "earliest". (If run_time of tasks
            // are in descending orders, all tasks are "earliest" after every
            // insertion, and they'll grab _mutex and change _nearest_run_time
            // frequently, fortunately this is not true at most of time).
            {
                FLARE_SCOPED_LOCK(_mutex);
                if (task1->run_time > _nearest_run_time) {
                    // a task is earlier than task1. We need to check buckets.
                    pull_again = true;
                    break;
                }
            }
            std::pop_heap(tasks.begin(), tasks.end(), task_greater);
            tasks.pop_back();
            if (task1->run_and_delete()) {
                ++ntriggered;
            }
        }
        if (pull_again) {
            BT_VLOG << "pull again, tasks=" << tasks.size();
            continue;
        }

        // The realtime to wait for.
        int64_t next_run_time = std::numeric_limits<int64_t>::max();
        if (!tasks.empty()) {
            next_run_time = tasks[0]->run_time;
        }
        // Similarly with the situation before running tasks, we check
        // _nearest_run_time to prevent us from waiting on a non-earliest
        // task. We also use the _nsignal to make sure that if new task 
        // is earlier that the realtime that we wait for, we'll wake up.
        int expected_nsignals = 0;
        {
            FLARE_SCOPED_LOCK(_mutex);
            if (next_run_time > _nearest_run_time) {
                // a task is earlier that what we would wait for.
                // We need to check buckets.
                continue;
            } else {
                _nearest_run_time = next_run_time;
                expected_nsignals = _nsignals;
            }
        }
        timespec* ptimeout = NULL;
        timespec next_timeout = { 0, 0 };
        const int64_t now = flare::get_current_time_micros();
        if (next_run_time != std::numeric_limits<int64_t>::max()) {
            next_timeout = flare::duration::microseconds(next_run_time - now).to_timespec();
            ptimeout = &next_timeout;
        }
        busy_seconds += (now - last_sleep_time) / 1000000.0;
        futex_wait_private(&_nsignals, expected_nsignals, ptimeout);
        last_sleep_time = flare::get_current_time_micros();
    }
    BT_VLOG << "Ended TimerThread=" << pthread_self();
}

void TimerThread::stop_and_join() {
    _stop.store(true, std::memory_order_relaxed);
    if (_started) {
        {
            FLARE_SCOPED_LOCK(_mutex);
             // trigger pull_again and wakeup TimerThread
            _nearest_run_time = 0;
            ++_nsignals;
        }
        if (pthread_self() != _thread) {
            // stop_and_join was not called from a running task.
            // wake up the timer thread in case it is sleeping.
            futex_wake_private(&_nsignals, 1);
            pthread_join(_thread, NULL);
        }
    }
}

static pthread_once_t g_timer_thread_once = PTHREAD_ONCE_INIT;
static TimerThread* g_timer_thread = NULL;
static void init_global_timer_thread() {
    g_timer_thread = new (std::nothrow) TimerThread;
    if (g_timer_thread == NULL) {
        FLARE_LOG(FATAL) << "Fail to new g_timer_thread";
        return;
    }
    TimerThreadOptions options;
    options.variable_prefix = "fiber_timer";
    const int rc = g_timer_thread->start(&options);
    if (rc != 0) {
        FLARE_LOG(FATAL) << "Fail to start timer_thread, " << flare_error(rc);
        delete g_timer_thread;
        g_timer_thread = NULL;
        return;
    }
}
TimerThread* get_or_create_global_timer_thread() {
    pthread_once(&g_timer_thread_once, init_global_timer_thread);
    return g_timer_thread;
}
TimerThread* get_global_timer_thread() {
    return g_timer_thread;
}

}  // end namespace flare::fiber_internal
