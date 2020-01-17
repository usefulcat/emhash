
// emhash8::HashSet for C++11
// version 1.3.1
// https://github.com/ktprime/ktprime/blob/master/hash_set.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2020 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE


// From
// NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
// Quadratic collision resolution   1 - ln(1-L) - L/2    1/(1-L) - L - ln(1-L)
// Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)2]/2
//
// -- enlarge_factor --           0.10  0.50  0.60  0.75  0.80  0.90  0.99
// QUADRATIC COLLISION RES.
//    probes/successful lookup    1.05  1.44  1.62  2.01  2.21  2.85  5.11
//    probes/unsuccessful lookup  1.11  2.19  2.82  4.64  5.81  11.4  103.6
// LINEAR COLLISION RES.
//    probes/successful lookup    1.06  1.5   1.75  2.5   3.0   5.5   50.5
//    probes/unsuccessful lookup  1.12  2.5   3.6   8.5   13.0  50.0

#pragma once

#include <cstring>
#include <cstdlib>
#include <type_traits>
#include <cassert>
#include <utility>
#include <cstdint>
#include <functional>
#include <iterator>

#ifdef  GET_KEY
    #undef  hash_bucket
    #undef  NEXT_BUCKET
    #undef  GET_KEY
    #undef  NEW_KEY
#endif

// likely/unlikely
#if (__GNUC__ >= 4 || __clang__)
#    define EMHASH_LIKELY(condition) __builtin_expect(condition, 1)
#    define EMHASH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMHASH_LIKELY(condition) condition
#    define EMHASH_UNLIKELY(condition) condition
#endif

#define NEW_KEY(key, bucket) new(_pairs + bucket) PairT(key, bucket), _num_filled ++
#define CLEAR_BUCKET(_bucket)   _pairs[bucket].~PairT(); _num_filled --; NEXT_BUCKET(_pairs, bucket) = INACTIVE
#define hash_bucket(key)  ((uint32_t)_hasher(key) & _mask)

#if EMHASH_CACHE_LINE_SIZE < 32
    #define EMHASH_CACHE_LINE_SIZE 64
#endif

#define GET_KEY(p,n)     p[n].first
#define NEXT_BUCKET(p,n) p[n].second

namespace emhash8 {
/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashSet
{
    constexpr static uint32_t INACTIVE = 0xFFFFFFFF;

private:
    typedef  HashSet<KeyT, HashT> htype;
    typedef  std::pair<KeyT, uint32_t> PairT;

public:
    typedef size_t   size_type;
    typedef KeyT     key_type;
    typedef KeyT     value_type;
    typedef KeyT&    reference;
    typedef const KeyT& const_reference;

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef size_t                    difference_type;
        typedef KeyT                      value_type;
        typedef value_type*               pointer;
        typedef value_type&               reference;

        iterator() { }
        iterator(htype* hash_set, uint32_t bucket) : _set(hash_set), _bucket(bucket) { }

        iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
            auto old_index = _bucket;
            goto_next_element();
            return {_set, old_index};
        }

        reference operator*() const
        {
            return _set->GET_KEY(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_set->GET_KEY(_pairs, _bucket));
        }

        bool operator==(const iterator& rhs) const
        {
            return _bucket == rhs._bucket;
        }

        bool operator!=(const iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
            do {
                _bucket++;
            } while (_set->NEXT_BUCKET(_pairs, _bucket) == INACTIVE);
        }

    public:
        htype* _set;
        uint32_t  _bucket;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef size_t                    difference_type;
        typedef const KeyT                value_type;
        typedef value_type*               pointer;
        typedef value_type&               reference;

        const_iterator() { }
        const_iterator(iterator proto) : _set(proto._set), _bucket(proto._bucket) {  }
        const_iterator(const htype* hash_set, uint32_t bucket) : _set(hash_set), _bucket(bucket) {  }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            auto old_index = _bucket;
            goto_next_element();
            return {_set, old_index};
        }

        reference operator*() const
        {
            return _set->GET_KEY(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_set->GET_KEY(_pairs, _bucket));
        }

        bool operator==(const const_iterator& rhs) const
        {
            return _bucket == rhs._bucket;
        }

        bool operator!=(const const_iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
            do {
                _bucket++;
            } while (_set->NEXT_BUCKET(_pairs, _bucket) == INACTIVE);
        }

    public:
        const htype* _set;
        uint32_t  _bucket;
    };

    // ------------------------------------------------------------------------

    void init()
    {
        _num_buckets = 0;
        _max_bucket = 0;
        _last_colls = 0;
        _mask = 0;
        _pairs = nullptr;
        _num_filled = 0;
#if EMHASH_HIGH_LOAD
        max_load_factor(7.0/8.0f);
#else
        max_load_factor(9.0/10.0f);
#endif
    }

    HashSet(uint32_t bucket = 4)
    {
        init();
        reserve(bucket);
    }

    HashSet(const HashSet& other)
    {
        _pairs = (PairT*)malloc((2 + other._max_bucket) * sizeof(PairT));
        clone(other);
    }

    void clone(const HashSet& other)
    {
        _hasher      = other._hasher;
        _num_buckets = other._num_buckets;
        _num_filled  = other._num_filled;
        _max_bucket  = other._max_bucket;
        _last_colls  = other._last_colls;
        _mask        = other._mask;
        _loadlf      = other._loadlf;

        if (std::is_pod<KeyT>::value) {
            memcpy(_pairs, other._pairs, _max_bucket * sizeof(PairT));
        } else {
            auto old_pairs = other._pairs;
            for (uint32_t bucket = 0; bucket < _max_bucket; bucket++) {
                auto next_bucket = NEXT_BUCKET(_pairs, bucket) = NEXT_BUCKET(old_pairs, bucket);
                if (next_bucket != INACTIVE)
                    new(_pairs + bucket) PairT(old_pairs[bucket]);
            }
        }
        NEXT_BUCKET(_pairs, _max_bucket) =  NEXT_BUCKET(_pairs, _max_bucket + 1) = 0;
    }

    HashSet(HashSet&& other)
    {
        init();
        reserve(1);
        *this = std::move(other);
    }

    HashSet(std::initializer_list<value_type> il, int n = 8)
    {
        init();
        reserve((uint32_t)il.size());
        for (auto begin = il.begin(); begin != il.end(); ++begin)
            insert(*begin);
    }

    HashSet& operator=(const HashSet& other)
    {
        if (this == &other)
            return *this;

        if (!std::is_pod<KeyT>::value)
            clearkv();

        if (_max_bucket != other._max_bucket) {
            free(_pairs);
            _pairs = (PairT*)malloc((2 + other._max_bucket) * sizeof(PairT));
        }

        clone(other);
        return *this;
    }

    HashSet& operator=(HashSet&& other)
    {
        swap(other);
        return *this;
    }

    ~HashSet()
    {
        if (!std::is_pod<KeyT>::value)
            clearkv();

        free(_pairs);
    }

    void swap(HashSet& other)
    {
        std::swap(_hasher, other._hasher);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_max_bucket, other._max_bucket);
        std::swap(_last_colls, other._last_colls);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
        std::swap(_loadlf, other._loadlf);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        uint32_t bucket = 0;
        while (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
            ++bucket;
        }
        return {this, bucket};
    }

    const_iterator cbegin() const
    {
        uint32_t bucket = 0;
        while (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
            ++bucket;
        }
        return {this, bucket};
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    iterator end()
    {
        return {this, _max_bucket};
    }

    const_iterator cend() const
    {
        return {this, _max_bucket};
    }

    const_iterator end() const
    {
        return {this, _max_bucket};
    }

    size_type size() const
    {
        return _num_filled;
    }

    bool empty() const
    {
        return _num_filled == 0;
    }

    // Returns the number of buckets.
    size_type bucket_count() const
    {
        return _num_buckets;
    }

    /// Returns average number of elements per bucket.
    float load_factor() const
    {
        return static_cast<float>(_num_filled) / (_num_buckets + 1);
    }

    HashT hash_function() const
    {
        return _hasher;
    }

    EqT key_eq() const
    {
        return _eq;
    }

    constexpr float max_load_factor() const
    {
        return (1 << 13) /(float)_loadlf;
    }

    void max_load_factor(float value)
    {
        if (value < 0.95f && value > 0.2f)
            _loadlf = (uint32_t)((1 << 13) / value);
    }

    constexpr size_type max_size() const
    {
        return (1 << 30) / sizeof(PairT);
    }

    constexpr size_type max_bucket_count() const
    {
        return (1 << 30) / sizeof(PairT);
    }

    //Returns the bucket number where the element with key k is located.
    size_type bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key);
        const auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return 0;
        else if (bucket == next_bucket)
            return bucket + 1;

        const auto& bucket_key = GET_KEY(_pairs, bucket);
        return hash_bucket(bucket_key) + 1;
    }

    //Returns the number of elements in bucket n.
    size_type bucket_size(const size_type bucket) const
    {
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return 0;

        const auto& bucket_key = GET_KEY(_pairs, bucket);
        next_bucket = hash_bucket(bucket_key);
        uint32_t ibucket_size = 1;

        //iterator each item in current main bucket
        while (true) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                break;
            }
            ibucket_size ++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

#ifdef EMHASH_STATIS
    size_type get_main_bucket(const uint32_t bucket) const
    {
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto& bucket_key = GET_KEY(_pairs, bucket);
        const auto main_bucket = hash_bucket(bucket_key);
        return main_bucket;
    }

    int get_cache_info(uint32_t bucket, uint32_t next_bucket) const
    {
        auto pbucket = reinterpret_cast<size_t>(&_pairs[bucket]);
        auto pnext   = reinterpret_cast<size_t>(&_pairs[next_bucket]);
        if (pbucket / 64 == pnext / 64)
            return 0;
        auto diff = pbucket > pnext ? (pbucket - pnext) : pnext - pbucket;
        if (diff < 127 * 64)
            return diff / 64 + 1;
        return 127;
    }

    int get_bucket_info(const uint32_t bucket, uint32_t steps[], const uint32_t slots) const
    {
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return -1;

        const auto& bucket_key = GET_KEY(_pairs, bucket);
        const auto main_bucket = hash_bucket(bucket_key);
        if (main_bucket != bucket)
            return 0;
        else if (next_bucket == bucket)
            return 1;

        steps[get_cache_info(bucket, next_bucket) % slots] ++;
        uint32_t ibucket_size = 2;
        //find a new empty and linked it to tail
        while (true) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;

            steps[get_cache_info(nbucket, next_bucket) % slots] ++;
            ibucket_size ++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

    void dump_statis() const
    {
        uint32_t buckets[129] = {0};
        uint32_t steps[129]   = {0};
        for (uint32_t bucket = 0; bucket < _num_buckets; ++bucket) {
            auto bsize = get_bucket_info(bucket, steps, 128);
            if (bsize > 0)
                buckets[bsize] ++;
        }

        uint32_t sumb = 0, collision = 0, sumc = 0, finds = 0, sumn = 0;
        puts("============== buckets size ration =========");
        for (uint32_t i = 0; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
            const auto bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;
            sumb += bucketsi;
            sumn += bucketsi * i;
            collision += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            printf("  %2u  %8u  %.2lf  %.2lf\n", i, bucketsi, bucketsi * 100.0 * i / _num_filled, sumn * 100.0 / _num_filled);
        }

        puts("========== collision miss ration ===========");
        for (uint32_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
            sumc += steps[i];
            if (steps[i] <= 2)
                continue;
            printf("  %2u  %8u  %.2lf  %.2lf\n", i, steps[i], steps[i] * 100.0 / collision, sumc * 100.0 / collision);
        }

        if (sumb == 0)  return;
        printf("    _num_filled/bucket_size/packed collision/cache_miss/hit_find = %u/%.2lf/%zd/ %.2lf%%/%.2lf%%/%.2lf\n",
                _num_filled, _num_filled * 1.0 / sumb, sizeof(PairT), (collision * 100.0 / _num_filled), (collision - steps[0]) * 100.0 / _num_filled, finds * 1.0 / _num_filled);
        assert(sumn == _num_filled);
        assert(sumc == collision);
    }
#endif

    // ------------------------------------------------------------

    iterator find(const KeyT& key)
    {
        return {this, find_filled_bucket(key)};
    }

    const_iterator find(const KeyT& key) const
    {
        return {this, find_filled_bucket(key)};
    }

    bool contains(const KeyT& key) const
    {
        return find_filled_bucket(key) != _max_bucket;
    }

    size_type count(const KeyT& key) const
    {
        return find_filled_bucket(key) == _max_bucket ? 0 : 1;
    }

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
            NEW_KEY(key, bucket);
            return { {this, bucket}, true };
        } else {
            return { {this, bucket}, false };
        }
    }

    std::pair<iterator, bool> insert(KeyT&& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
            NEW_KEY(std::move(key), bucket);
            return { {this, bucket}, true };
        } else {
            return { {this, bucket}, false };
        }
    }

#if 0
    template <typename Iter>
    inline void insert(Iter begin, Iter end)
    {
        reserve(end - begin + _num_filled);
        for (; begin != end; ++begin) {
            insert(*begin);
        }
    }
#endif

    void insert(std::initializer_list<value_type> ilist)
    {
        reserve((uint32_t)ilist.size() + _num_filled);
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin) {
            insert(*begin);
        }
    }

    template <typename Iter>
    inline void insert(Iter begin, Iter end)
    {
        Iter citbeg = begin;
        Iter citend = begin;
        reserve(end - begin + _num_filled);
        for (; begin != end; ++begin) {
            if (try_insert_mainbucket(*begin) == INACTIVE) {
                std::swap(*begin, *citend++);
            }
        }

        for (; citbeg != citend; ++citbeg) {
            auto& key = *citbeg;
            const auto bucket = find_or_allocate(key);
            if (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
                NEW_KEY(key, bucket);
            }
        }
    }

    template <typename Iter>
    inline void insert_unique(Iter begin, Iter end)
    {
        reserve(end - begin + _num_filled);
        for (; begin != end; ++begin) {
            insert_unique(*begin);
        }
    }

    /// Same as above, but contains(key) MUST be false
    uint32_t insert_unique(const KeyT& key)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        NEW_KEY(key, bucket);
        return bucket;
    }

    uint32_t insert_unique(KeyT&& key)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        NEW_KEY(std::move(key), bucket);
        return bucket;
    }

    //not
    template <class... Args>
    inline std::pair<iterator, bool> emplace(Args&&... args)
    {
        return insert(std::forward<Args>(args)...);
    }

    //no any optimize for position
    template <class... Args>
    iterator emplace_hint(const_iterator position, Args&&... args)
    {
        return insert(std::forward<Args>(args)...).first;
    }
    std::pair<iterator, bool> try_emplace(const value_type& k)
    {
        return insert(k).first;
    }
    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }

    uint32_t try_insert_mainbucket(const KeyT& key)
    {
        auto bucket = hash_bucket(key);
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE) {
            NEW_KEY(key, bucket);
            return bucket;
        } else if(_eq(key, GET_KEY(_pairs, bucket)))
            return bucket;

        return INACTIVE;
    }

    void insert_or_assign(const KeyT& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        // Check if inserting a new value rather than overwriting an old entry
        if (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
            NEW_KEY(key, bucket);
        } else {
            NEXT_BUCKET(_pairs, bucket) = bucket;
        }
    }

    // -------------------------------------------------------
    //reset last_bucket collision bucket to bucket
    void erase_coll(uint32_t bucket)
    {
        const auto last_bucket = ++ _last_colls;
        if (bucket == last_bucket)
            return;

        const auto last_next = NEXT_BUCKET(_pairs, last_bucket);
        auto& key = GET_KEY(_pairs, last_bucket);
        const auto main_bucket = hash_bucket(key);
        const auto prev_bucket = find_prev_bucket(main_bucket, last_bucket);

        new(_pairs + bucket) PairT(std::move(key), last_next != last_bucket ? last_next : bucket);

        NEXT_BUCKET(_pairs, prev_bucket) = bucket;
        NEXT_BUCKET(_pairs, last_bucket) = INACTIVE;
        _pairs[last_bucket].~PairT();
    }

    /// Erase an element from the hash table.
    /// return 0 if element was not found
    size_type erase(const KeyT& key)
    {
        const auto bucket = erase_key(key);
        if (bucket == INACTIVE)
            return 0;

        CLEAR_BUCKET(bucket);
#if EMHASH_HIGH_LOAD
        if (bucket >= _last_colls)
            erase_coll(bucket);
#endif
        return 1;
    }

    iterator erase(const_iterator cit)
    {
        iterator it(this, cit._bucket);
        return erase(it);
    }

    /// Erase an element typedef an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it)
    {
#if _DEBUG
        // we assume that it always points to a valid entry, and not end().
        assert(this == it._set);
        if (it._bucket >= _max_bucket)
            return end();
        else if (INACTIVE == NEXT_BUCKET(_pairs, it._bucket))
            return ++it;
        assert(*it == GET_KEY(_pairs, it._bucket));
#endif

        const auto bucket = erase_bucket(it._bucket);
        CLEAR_BUCKET(bucket);
        //move last bucket to current
#if EMHASH_HIGH_LOAD
        if (bucket >= _last_colls)
            erase_coll(bucket);
#endif

        //erase from main bucket, return main bucket as next
        return (bucket == it._bucket) ? ++it : it;

    }

    void _erase(iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        CLEAR_BUCKET(bucket);
 #if EMHASH_HIGH_LOAD
        if (bucket >= _last_colls)
            erase_coll(bucket);
#endif
    }

    void clearkv()
    {
        for (uint32_t bucket = 0; _num_filled > 0; ++bucket) {
            if (NEXT_BUCKET(_pairs, bucket) != INACTIVE) {
                CLEAR_BUCKET(bucket);
            }
        }
        _last_colls = _max_bucket - 1;
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (_num_filled > _max_bucket / 4 && std::is_pod<KeyT>::value)
            memset(_pairs, INACTIVE, sizeof(_pairs[0]) * _max_bucket);
        else
            clearkv();
        _num_filled = 0;
        _last_colls = _max_bucket - 1;
    }

    void shrink_to_fit() noexcept
    {
        rehash(_num_filled);
    }

    /// Make room for this many elements
    bool reserve(uint32_t num_elems)
    {
#ifdef EMHASH_HIGH_LOAD
        const auto required_buckets = num_elems + num_elems / 8;
#else
        const auto required_buckets = (uint32_t)(((uint64_t)num_elems * _loadlf) >> 13) + 2;
#endif

        if (EMHASH_LIKELY(required_buckets < _max_bucket))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    /// Make room for this many elements
    void rehash(uint32_t required_buckets)
    {
        if (required_buckets < _num_filled)
            return ;

        uint32_t num_buckets = _num_filled > 65536 ? (1 << 16) : 4;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        const auto old_max_bucket = _max_bucket;

#ifdef EMHASH_HIGH_LOAD
        _max_bucket = num_buckets + num_buckets / 8;
#else
        _max_bucket  = num_buckets;
#endif

        //assert(num_buckets > _num_filled);
        auto new_pairs = (PairT*)malloc((2 + _max_bucket) * sizeof(PairT));
        auto old_num_filled  = _num_filled;
//      auto old_num_buckets = _num_buckets;
        auto old_pairs = _pairs;
        auto old_last_colls = _last_colls;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;

        _last_colls  = _num_buckets;
        _pairs       = new_pairs;

        if (sizeof(PairT) <= EMHASH_CACHE_LINE_SIZE / 2) {
            memset(_pairs, INACTIVE, sizeof(_pairs[0]) * _max_bucket);
        } else {
            for (uint32_t bucket = 0; bucket < _max_bucket; bucket++)
                NEXT_BUCKET(_pairs, bucket) = INACTIVE;
        }
        NEXT_BUCKET(_pairs, _max_bucket) =  NEXT_BUCKET(_pairs, _max_bucket + 1) = 0;

        uint32_t collision = 0;
        //set all main bucket first
        for (uint32_t src_bucket = 0; src_bucket < old_max_bucket && old_num_filled > 0; src_bucket++) {
            if (NEXT_BUCKET(old_pairs, src_bucket) == INACTIVE)
                continue;

            auto& key = GET_KEY(old_pairs, src_bucket);
            const auto bucket = find_unique_bucket(key);
            NEW_KEY(std::move(key), bucket);
            old_pairs[src_bucket].~PairT();
        }

#ifdef EMHASH_HIGH_LOAD
        _last_colls = _max_bucket - 1;
#endif

#if EMHASH_REHASH_LOG
        if (_num_filled > 100000) {
            auto mbucket = _num_filled - collision;
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/aver_size/K/pack/collision = %u/%.2lf/%s/%zd/%.2lf%%",
                    _num_filled, (double)_num_filled / mbucket, typeid(KeyT).name(), sizeof(_pairs[0]), (collision * 100.0 / _num_filled));
#if EMHASH_TAF_LOG
            static uint32_t ihashs = 0;
            FDLOG() << "|hash_nums = " << ihashs ++ << "|" <<__FUNCTION__ << "|" << buff << endl;
#else
            puts(buff);
#endif
        }
#endif

        free(old_pairs);
        assert(old_num_filled == _num_filled);
    }

private:
    // Can we fit another element?
    inline bool check_expand_need()
    {
        return reserve(_num_filled);
    }

    uint32_t erase_key(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto eqkey = _eq(key, GET_KEY(_pairs, bucket));
        if (next_bucket == bucket) {
            return eqkey ? bucket : INACTIVE;
         } else if (eqkey) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (std::is_pod<KeyT>::value)
                GET_KEY(_pairs, bucket) = GET_KEY(_pairs, next_bucket);
            else
                std::swap(GET_KEY(_pairs, bucket), GET_KEY(_pairs, next_bucket));
            NEXT_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            return next_bucket;
        } else if (EMHASH_UNLIKELY(bucket != hash_bucket(GET_KEY(_pairs, bucket))))
            return INACTIVE;

        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (_eq(key, GET_KEY(_pairs, next_bucket))) {
                NEXT_BUCKET(_pairs, prev_bucket) = (nbucket == next_bucket) ? prev_bucket : nbucket;
                return next_bucket;
            }

            if (nbucket == next_bucket)
                break;
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return INACTIVE;
    }

    uint32_t erase_bucket(const uint32_t bucket)
    {
        const auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        const auto main_bucket = hash_bucket(GET_KEY(_pairs, bucket));
        if (bucket == main_bucket) {
            if (bucket != next_bucket) {
                const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
                if (std::is_pod<KeyT>::value)
                    GET_KEY(_pairs, bucket) = GET_KEY(_pairs, next_bucket);
                else
                    std::swap(GET_KEY(_pairs, bucket), GET_KEY(_pairs, next_bucket));
                NEXT_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            }
            return next_bucket;
        }

        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        NEXT_BUCKET(_pairs, prev_bucket) = (bucket == next_bucket) ? prev_bucket : next_bucket;
        return bucket;
    }

    // Find the bucket with this key, or return bucket size
    uint32_t find_filled_bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        const auto& bucket_key = GET_KEY(_pairs, bucket);
        if (next_bucket == INACTIVE) // || bucket != hash_bucket(bucket_key))
            return _max_bucket;
        else if (_eq(key, bucket_key))
            return bucket;
        else if (next_bucket == bucket)
            return _max_bucket;

        //find next linked bucket
        while (true) {
            if (_eq(key, GET_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        return _max_bucket;
    }

    uint32_t kickout_bucket(const uint32_t main_bucket, const uint32_t bucket)
    {
        const auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        NEXT_BUCKET(_pairs, prev_bucket) = new_bucket;
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket])); _pairs[bucket].~PairT();
        if (next_bucket == bucket)
            NEXT_BUCKET(_pairs, new_bucket) = new_bucket;
        NEXT_BUCKET(_pairs, bucket) = INACTIVE;
        return bucket;
    }

/*
** inserts a new key into a hash table; first, check whether key's main
** bucket/position is free. If not, check whether colliding node/bucket is in its main
** position or not: if it is not, move colliding bucket to an empty place and
** put new key in its main position; otherwise (colliding bucket is in its main
** position), new key goes to an empty position.
*/
    uint32_t find_or_allocate(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        const auto& bucket_key = GET_KEY(_pairs, bucket);
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE || _eq(key, bucket_key))
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto main_bucket = hash_bucket(bucket_key);
        if (main_bucket != bucket)
            return kickout_bucket(main_bucket, bucket);
        else if (next_bucket == bucket)
            return NEXT_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);

        //find next linked bucket and check key
        while (true) {
            if (_eq(key, GET_KEY(_pairs, next_bucket))) {
#if EMHASH_LRU_SET
                std::swap(GET_KEY(_pairs, bucket), GET_KEY(_pairs, next_bucket));
                return bucket;
#else
                return next_bucket;
#endif
            }

            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        //find a new empty and link it to tail
        const auto new_bucket = find_empty_bucket(next_bucket);
        return NEXT_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    // key is not in this map. Find a place to put it.
    uint32_t find_empty_bucket(uint32_t bucket_from)
    {
        const auto bucket1 = bucket_from + 1;
        if (NEXT_BUCKET(_pairs, bucket1) == INACTIVE)
            return bucket1;

        const auto bucket2 = bucket_from + 2;
        if (NEXT_BUCKET(_pairs, bucket2) == INACTIVE)
            return bucket2;

#ifdef EMHASH_HIGH_LOAD
        //if (bucket_from > _num_buckets && INACTIVE == NEXT_BUCKET(_pairs, _last_colls)) return _last_colls --;
        if (INACTIVE == NEXT_BUCKET(_pairs, _last_colls)) return _last_colls --;
#endif

        //fibonacci an2 = an1 + an0 --> 1, 2, 3, 5, 8, 13, 21 ...
        //for (uint32_t last = 2, slot = 3; ; slot += last, last = slot - last) {
        for (uint32_t last = 2, slot = 3; ; last ++, slot += last) {
            const auto bucket = (bucket_from + slot) & _mask;
            if (NEXT_BUCKET(_pairs, bucket) == INACTIVE)
                return bucket;

            const auto bucket2 = bucket + 1;
            if (NEXT_BUCKET(_pairs, bucket2) == INACTIVE)
                return bucket2;
#if 0
            if (last > 5) {
                const auto next = (bucket_from + _num_filled + last) & _mask;
                const auto bucket3 = next + 0;
                if (NEXT_BUCKET(_pairs, bucket3) == INACTIVE)
                    return bucket3;
            }
#endif
        }
    }

    uint32_t find_last_bucket(uint32_t main_bucket) const
    {
        auto next_bucket = NEXT_BUCKET(_pairs, main_bucket);
        if (next_bucket == main_bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    uint32_t find_prev_bucket(uint32_t main_bucket, const uint32_t bucket) const
    {
        auto next_bucket = NEXT_BUCKET(_pairs, main_bucket);
        if (next_bucket == bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    uint32_t find_unique_bucket(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto main_bucket = hash_bucket(GET_KEY(_pairs, bucket));
        if (main_bucket != bucket)
            return kickout_bucket(main_bucket, bucket);
        else if (next_bucket != bucket)
            next_bucket = find_last_bucket(next_bucket);

        //find a new empty and link it to tail
        return NEXT_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);
    }

private:

    //the first cache line packed
    HashT     _hasher;
    EqT       _eq;
    uint32_t  _loadlf;
    uint32_t  _num_buckets;
    uint32_t  _mask;
    uint32_t  _max_bucket;

    uint32_t  _last_colls;
    uint32_t  _num_filled;

    PairT*    _pairs;
};
} // namespace emhash
#if __cplusplus >= 201103L
template <class Key, typename Hash = std::hash<Key>> using ktprime_hashset_v8 = emhash8::HashSet<Key, Hash>;
#endif
