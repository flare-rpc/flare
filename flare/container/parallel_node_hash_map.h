//
// Created by liyinbin on 2022/5/15.
//

#ifndef FLARE_CONTAINER_PARALLEL_NODE_HASH_MAP_H_
#define FLARE_CONTAINER_PARALLEL_NODE_HASH_MAP_H_

#include "flare/container/internal/raw_hash_set.h"

namespace flare {
    // -----------------------------------------------------------------------------
    // flare::parallel_node_hash_map
    // -----------------------------------------------------------------------------
    template<class Key, class Value, class Hash, class Eq, class Alloc, size_t N, class Mtx_>
    class parallel_node_hash_map
            : public flare::priv::parallel_hash_map<
                    N, flare::priv::raw_hash_set, Mtx_,
                    flare::priv::node_hash_map_policy<Key, Value>, Hash, Eq,
                    Alloc> {
        using Base = typename parallel_node_hash_map::parallel_hash_map;

    public:
        parallel_node_hash_map() {}

#ifdef __INTEL_COMPILER
        using Base::parallel_hash_map;
#else
        using Base::Base;
#endif
        using Base::hash;
        using Base::subidx;
        using Base::subcnt;
        using Base::begin;
        using Base::cbegin;
        using Base::cend;
        using Base::end;
        using Base::capacity;
        using Base::empty;
        using Base::max_size;
        using Base::size;
        using Base::clear;
        using Base::erase;
        using Base::insert;
        using Base::insert_or_assign;
        using Base::emplace;
        using Base::emplace_hint;
        using Base::try_emplace;
        using Base::emplace_with_hash;
        using Base::emplace_hint_with_hash;
        using Base::try_emplace_with_hash;
        using Base::extract;
        using Base::merge;
        using Base::swap;
        using Base::rehash;
        using Base::reserve;
        using Base::at;
        using Base::contains;
        using Base::count;
        using Base::equal_range;
        using Base::find;
        using Base::operator[];
        using Base::bucket_count;
        using Base::load_factor;
        using Base::max_load_factor;
        using Base::get_allocator;
        using Base::hash_function;
        using Base::key_eq;

        typename Base::hasher hash_funct() { return this->hash_function(); }

        void resize(typename Base::size_type hint) { this->rehash(hint); }
    };

    template<class Key, class Value, class Hash, class Eq, class Alloc, size_t N, class Mtx_>
    class case_ignored_parallel_node_hash_map
            : public flare::priv::parallel_hash_map<
                    N, flare::priv::raw_hash_set, Mtx_,
                    flare::priv::node_hash_map_policy<Key, Value>, Hash, Eq,
                    Alloc> {
        using Base = typename case_ignored_parallel_node_hash_map::parallel_hash_map;

    public:
        case_ignored_parallel_node_hash_map() {}

#ifdef __INTEL_COMPILER
        using Base::parallel_hash_map;
#else
        using Base::Base;
#endif
        using Base::hash;
        using Base::subidx;
        using Base::subcnt;
        using Base::begin;
        using Base::cbegin;
        using Base::cend;
        using Base::end;
        using Base::capacity;
        using Base::empty;
        using Base::max_size;
        using Base::size;
        using Base::clear;
        using Base::erase;
        using Base::insert;
        using Base::insert_or_assign;
        using Base::emplace;
        using Base::emplace_hint;
        using Base::try_emplace;
        using Base::emplace_with_hash;
        using Base::emplace_hint_with_hash;
        using Base::try_emplace_with_hash;
        using Base::extract;
        using Base::merge;
        using Base::swap;
        using Base::rehash;
        using Base::reserve;
        using Base::at;
        using Base::contains;
        using Base::count;
        using Base::equal_range;
        using Base::find;
        using Base::operator[];
        using Base::bucket_count;
        using Base::load_factor;
        using Base::max_load_factor;
        using Base::get_allocator;
        using Base::hash_function;
        using Base::key_eq;

        typename Base::hasher hash_funct() { return this->hash_function(); }

        void resize(typename Base::size_type hint) { this->rehash(hint); }
    };


}  // namespace flare
#endif  // FLARE_CONTAINER_PARALLEL_NODE_HASH_MAP_H_
