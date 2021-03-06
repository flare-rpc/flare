

#ifndef FLARE_CONTAINER_DOUBLY_BUFFERED_DATA_H_
#define FLARE_CONTAINER_DOUBLY_BUFFERED_DATA_H_

#include <vector>                                       // std::vector
#include <pthread.h>
#include "flare/base/scoped_lock.h"
#include "flare/thread/thread.h"
#include "flare/log/logging.h"
#include "flare/base/type_traits.h"
#include "flare/base/errno.h"
#include "flare/base/static_atomic.h"
#include <memory>

namespace flare::container {

    // This data structure makes Read() almost lock-free by making Modify()
    // *much* slower. It's very suitable for implementing LoadBalancers which
    // have a lot of concurrent read-only ops from many threads and occasional
    // modifications of data. As a side effect, this data structure can store
    // a thread-local data for user.
    //
    // Read(): begin with a thread-local mutex locked then read the foreground
    // instance which will not be changed before the mutex is unlocked. Since the
    // mutex is only locked by Modify() with an empty critical section, the
    // function is almost lock-free.
    //
    // Modify(): Modify background instance which is not used by any Read(), flip
    // foreground and background, lock thread-local mutexes one by one to make
    // sure all existing Read() finish and later Read() see new foreground,
    // then modify background(foreground before flip) again.

    class Void {
    };

    template<typename T, typename TLS = Void>
    class DoublyBufferedData {
        class Wrapper;

    public:
        class ScopedPtr {
            friend class DoublyBufferedData;

        public:
            ScopedPtr() : _data(NULL), _w(NULL) {}

            ~ScopedPtr() {
                if (_w) {
                    _w->EndRead();
                }
            }

            const T *get() const { return _data; }

            const T &operator*() const { return *_data; }

            const T *operator->() const { return _data; }

            TLS &tls() { return _w->user_tls(); }

        private:
            FLARE_DISALLOW_COPY_AND_ASSIGN(ScopedPtr);

            const T *_data;
            Wrapper *_w;
        };

        DoublyBufferedData();

        ~DoublyBufferedData();

        // Put foreground instance into ptr. The instance will not be changed until
        // ptr is destructed.
        // This function is not blocked by Read() and Modify() in other threads.
        // Returns 0 on success, -1 otherwise.
        int Read(ScopedPtr *ptr);

        // Modify background and foreground instances. fn(T&, ...) will be called
        // twice. Modify() from different threads are exclusive from each other.
        // NOTE: Call same series of fn to different equivalent instances should
        // result in equivalent instances, otherwise foreground and background
        // instance will be inconsistent.
        template<typename Fn>
        size_t Modify(Fn &fn);

        template<typename Fn, typename Arg1>
        size_t Modify(Fn &fn, const Arg1 &);

        template<typename Fn, typename Arg1, typename Arg2>
        size_t Modify(Fn &fn, const Arg1 &, const Arg2 &);

        // fn(T& background, const T& foreground, ...) will be called to background
        // and foreground instances respectively.
        template<typename Fn>
        size_t ModifyWithForeground(Fn &fn);

        template<typename Fn, typename Arg1>
        size_t ModifyWithForeground(Fn &fn, const Arg1 &);

        template<typename Fn, typename Arg1, typename Arg2>
        size_t ModifyWithForeground(Fn &fn, const Arg1 &, const Arg2 &);

    private:

        template<typename Fn>
        struct WithFG0 {
            WithFG0(Fn &fn, T *data) : _fn(fn), _data(data) {}

            size_t operator()(T &bg) {
                return _fn(bg, (const T &) _data[&bg == _data]);
            }

        private:
            Fn &_fn;
            T *_data;
        };

        template<typename Fn, typename Arg1>
        struct WithFG1 {
            WithFG1(Fn &fn, T *data, const Arg1 &arg1)
                    : _fn(fn), _data(data), _arg1(arg1) {}

            size_t operator()(T &bg) {
                return _fn(bg, (const T &) _data[&bg == _data], _arg1);
            }

        private:
            Fn &_fn;
            T *_data;
            const Arg1 &_arg1;
        };

        template<typename Fn, typename Arg1, typename Arg2>
        struct WithFG2 {
            WithFG2(Fn &fn, T *data, const Arg1 &arg1, const Arg2 &arg2)
                    : _fn(fn), _data(data), _arg1(arg1), _arg2(arg2) {}

            size_t operator()(T &bg) {
                return _fn(bg, (const T &) _data[&bg == _data], _arg1, _arg2);
            }

        private:
            Fn &_fn;
            T *_data;
            const Arg1 &_arg1;
            const Arg2 &_arg2;
        };

        template<typename Fn, typename Arg1>
        struct Closure1 {
            Closure1(Fn &fn, const Arg1 &arg1) : _fn(fn), _arg1(arg1) {}

            size_t operator()(T &bg) { return _fn(bg, _arg1); }

        private:
            Fn &_fn;
            const Arg1 &_arg1;
        };

        template<typename Fn, typename Arg1, typename Arg2>
        struct Closure2 {
            Closure2(Fn &fn, const Arg1 &arg1, const Arg2 &arg2)
                    : _fn(fn), _arg1(arg1), _arg2(arg2) {}

            size_t operator()(T &bg) { return _fn(bg, _arg1, _arg2); }

        private:
            Fn &_fn;
            const Arg1 &_arg1;
            const Arg2 &_arg2;
        };

        const T *UnsafeRead() const { return _data + _index.load(std::memory_order_acquire); }

        Wrapper *AddWrapper();

        void RemoveWrapper(Wrapper *);

        // Foreground and background void.
        T _data[2];

        // Index of foreground instance.
        std::atomic<int> _index;

        // Key to access thread-local wrappers.
        bool _created_key;
        pthread_key_t _wrapper_key;

        // All thread-local instances.
        std::vector<Wrapper *> _wrappers;

        // Sequence access to _wrappers.
        pthread_mutex_t _wrappers_mutex;

        // Sequence modifications.
        pthread_mutex_t _modify_mutex;
    };

    static const pthread_key_t INVALID_PTHREAD_KEY = (pthread_key_t) -1;

    template<typename T, typename TLS>
    class DoublyBufferedDataWrapperBase {
    public:
        TLS &user_tls() { return _user_tls; }

    protected:
        TLS _user_tls;
    };

    template<typename T>
    class DoublyBufferedDataWrapperBase<T, Void> {
    };


    template<typename T, typename TLS>
    class DoublyBufferedData<T, TLS>::Wrapper
            : public DoublyBufferedDataWrapperBase<T, TLS> {
        friend class DoublyBufferedData;

    public:
        explicit Wrapper(DoublyBufferedData *c) : _control(c) {
            pthread_mutex_init(&_mutex, NULL);
        }

        ~Wrapper() {
            if (_control != NULL) {
                _control->RemoveWrapper(this);
            }
            pthread_mutex_destroy(&_mutex);
        }

        // _mutex will be locked by the calling pthread and DoublyBufferedData.
        // Most of the time, no modifications are done, so the mutex is
        // uncontended and fast.
        inline void BeginRead() {
            pthread_mutex_lock(&_mutex);
        }

        inline void EndRead() {
            pthread_mutex_unlock(&_mutex);
        }

        inline void WaitReadDone() {
            FLARE_SCOPED_LOCK(_mutex);
        }

    private:
        DoublyBufferedData *_control;
        pthread_mutex_t _mutex;
    };

// Called when thread initializes thread-local wrapper.
    template<typename T, typename TLS>
    typename DoublyBufferedData<T, TLS>::Wrapper *
    DoublyBufferedData<T, TLS>::AddWrapper() {
        std::unique_ptr<Wrapper> w(new(std::nothrow) Wrapper(this));
        if (NULL == w) {
            return NULL;
        }
        try {
            FLARE_SCOPED_LOCK(_wrappers_mutex);
            _wrappers.push_back(w.get());
        } catch (std::exception &e) {
            return NULL;
        }
        return w.release();
    }

// Called when thread quits.
    template<typename T, typename TLS>
    void DoublyBufferedData<T, TLS>::RemoveWrapper(
            typename DoublyBufferedData<T, TLS>::Wrapper *w) {
        if (NULL == w) {
            return;
        }
        FLARE_SCOPED_LOCK(_wrappers_mutex);
        for (size_t i = 0; i < _wrappers.size(); ++i) {
            if (_wrappers[i] == w) {
                _wrappers[i] = _wrappers.back();
                _wrappers.pop_back();
                return;
            }
        }
    }

    namespace detail {
        template<typename T>
        void delete_object(void *arg) {
            delete static_cast<T *>(arg);
        }
    }

    template<typename T, typename TLS>
    DoublyBufferedData<T, TLS>::DoublyBufferedData()
            : _index(0), _created_key(false), _wrapper_key(0) {
        _wrappers.reserve(64);
        pthread_mutex_init(&_modify_mutex, NULL);
        pthread_mutex_init(&_wrappers_mutex, NULL);
        const int rc = pthread_key_create(&_wrapper_key,
                                          detail::delete_object < Wrapper > );
        if (rc != 0) {
            FLARE_LOG(FATAL) << "Fail to pthread_key_create: " << flare_error(rc);
        } else {
            _created_key = true;
        }
        // Initialize _data for some POD types. This is essential for pointer
        // types because they should be Read() as NULL before any Modify().
        if (std::is_integral<T>::value || std::is_floating_point<T>::value ||
            std::is_pointer<T>::value || std::is_member_function_pointer<T>::value) {
            _data[0] = T();
            _data[1] = T();
        }
    }

    template<typename T, typename TLS>
    DoublyBufferedData<T, TLS>::~DoublyBufferedData() {
        // User is responsible for synchronizations between Read()/Modify() and
        // this function.
        if (_created_key) {
            pthread_key_delete(_wrapper_key);
        }

        {
            FLARE_SCOPED_LOCK(_wrappers_mutex);
            for (size_t i = 0; i < _wrappers.size(); ++i) {
                _wrappers[i]->_control = NULL;  // hack: disable removal.
                delete _wrappers[i];
            }
            _wrappers.clear();
        }
        pthread_mutex_destroy(&_modify_mutex);
        pthread_mutex_destroy(&_wrappers_mutex);
    }

    template<typename T, typename TLS>
    int DoublyBufferedData<T, TLS>::Read(
            typename DoublyBufferedData<T, TLS>::ScopedPtr *ptr) {
        if (FLARE_UNLIKELY(!_created_key)) {
            return -1;
        }
        Wrapper *w = static_cast<Wrapper *>(pthread_getspecific(_wrapper_key));
        if (FLARE_LIKELY(w != NULL)) {
            w->BeginRead();
            ptr->_data = UnsafeRead();
            ptr->_w = w;
            return 0;
        }
        w = AddWrapper();
        if (FLARE_LIKELY(w != NULL)) {
            const int rc = pthread_setspecific(_wrapper_key, w);
            if (rc == 0) {
                w->BeginRead();
                ptr->_data = UnsafeRead();
                ptr->_w = w;
                return 0;
            }
        }
        return -1;
    }

    template<typename T, typename TLS>
    template<typename Fn>
    size_t DoublyBufferedData<T, TLS>::Modify(Fn &fn) {
        // _modify_mutex sequences modifications. Using a separate mutex rather
        // than _wrappers_mutex is to avoid blocking threads calling
        // AddWrapper() or RemoveWrapper() too long. Most of the time, modifications
        // are done by one thread, contention should be negligible.
        FLARE_SCOPED_LOCK(_modify_mutex);
        int bg_index = !_index.load(std::memory_order_relaxed);
        // background instance is not accessed by other threads, being safe to
        // modify.
        const size_t ret = fn(_data[bg_index]);
        if (!ret) {
            return 0;
        }

        // Publish, flip background and foreground.
        // The release fence matches with the acquire fence in UnsafeRead() to
        // make readers which just begin to read the new foreground instance see
        // all changes made in fn.
        _index.store(bg_index, std::memory_order_release);
        bg_index = !bg_index;

        // Wait until all threads finishes current reading. When they begin next
        // read, they should see updated _index.
        {
            FLARE_SCOPED_LOCK(_wrappers_mutex);
            for (size_t i = 0; i < _wrappers.size(); ++i) {
                _wrappers[i]->WaitReadDone();
            }
        }

        const size_t ret2 = fn(_data[bg_index]);
        FLARE_CHECK_EQ(ret2, ret) << "index=" << _index.load(std::memory_order_relaxed);
        return ret2;
    }

    template<typename T, typename TLS>
    template<typename Fn, typename Arg1>
    size_t DoublyBufferedData<T, TLS>::Modify(Fn &fn, const Arg1 &arg1) {
        Closure1<Fn, Arg1> c(fn, arg1);
        return Modify(c);
    }

    template<typename T, typename TLS>
    template<typename Fn, typename Arg1, typename Arg2>
    size_t DoublyBufferedData<T, TLS>::Modify(
            Fn &fn, const Arg1 &arg1, const Arg2 &arg2) {
        Closure2<Fn, Arg1, Arg2> c(fn, arg1, arg2);
        return Modify(c);
    }

    template<typename T, typename TLS>
    template<typename Fn>
    size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(Fn &fn) {
        WithFG0<Fn> c(fn, _data);
        return Modify(c);
    }

    template<typename T, typename TLS>
    template<typename Fn, typename Arg1>
    size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(Fn &fn, const Arg1 &arg1) {
        WithFG1<Fn, Arg1> c(fn, _data, arg1);
        return Modify(c);
    }

    template<typename T, typename TLS>
    template<typename Fn, typename Arg1, typename Arg2>
    size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(
            Fn &fn, const Arg1 &arg1, const Arg2 &arg2) {
        WithFG2<Fn, Arg1, Arg2> c(fn, _data, arg1, arg2);
        return Modify(c);
    }

}  // namespace flare::container

#endif  // FLARE_CONTAINER_DOUBLY_BUFFERED_DATA_H_
