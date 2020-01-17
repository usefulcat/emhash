// emhash7::HashMap for C++11/14/17
// version 1.7.3
// https://github.com/ktprime/ktprime/blob/master/hash_table7.hpp
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
/** lf = 1
0    0.368
1    0.368
2    0.184
3    0.061
4    0.015
5    0.003
6    0.0005
============== buckets size ration poisson total ========
   1    766939  0.40633735|0.40656868  40.634
   2    344735  0.36529296|0.36591304  77.163
   3    103682  0.16479747|0.16466142  93.643
   4     23548  0.04990453|0.04939859  98.633
   5      4275  0.01132484|0.01111472  99.766
   6       625  0.00198681|0.00200066  99.964
   7        89  0.00033008|0.00030010  99.997
   8         5  0.00002119|0.00003858  100.000
   9         1  0.00000477|0.00000434  100.000
========== collision miss ration ===========
  _num_filled aver_size k.v size_kv = 1887444, 1.52, __int64.__int64 24
  collision,possion,cache_miss hit_find|hit_miss, load_factor = 34.10%,34.06%,27.11%  1.45|1.81, 0.90
========== collision miss ration =======================
   0    422679  57.54  57.54
   1    238081  32.41  89.95
   2     64649  8.80  98.75
   3      7527  1.02  99.77
   4      1336  0.18  99.95
   5       271  0.04  99.99
   6        47  0.01  100.00
   7        19  0.00  100.00
   8         5  0.00  100.00
  _num_filled aver_size k.v size_kv = 3774888, 1.24, l.i 16
  collision|possion_miss cache_miss hit_find load_factor = 19.46%|19.47% 8.26%, 1.22, 0.45
*******************************************************/

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
    #undef  GET_KEY
    #undef  GET_VAL
    #undef  GET_PKV
    #undef  hash_bucket
    #undef  NEXT_BUCKET
    #undef  NEW_KVALUE
#endif

// likely/unlikely
#if (__GNUC__ >= 4 || __clang__)
#    define EMHASH_LIKELY(condition)   __builtin_expect(condition, 1)
#    define EMHASH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMHASH_LIKELY(condition)   condition
#    define EMHASH_UNLIKELY(condition) condition
#endif

#ifndef EMHASH_BUCKET_INDEX
    #define EMHASH_BUCKET_INDEX 1
#endif
#if EMHASH_CACHE_LINE_SIZE < 32
    #define EMHASH_CACHE_LINE_SIZE 64
#endif

#if EMHASH_BUCKET_INDEX == 0
    #define GET_KEY(p,n)     p[n].second.first
    #define GET_VAL(p,n)     p[n].second.second
    #define NEXT_BUCKET(p,n) p[n].first
    #define GET_PKV(p,n)     p[n].second
    #define NEW_KVALUE(key, value, bucket) new(_pairs + bucket) PairT(bucket, std::pair<KeyT, ValueT>(key, value)); _num_filled ++
#elif EMHASH_BUCKET_INDEX == 2
    #define GET_KEY(p,n)     p[n].first.first
    #define GET_VAL(p,n)     p[n].first.second
    #define NEXT_BUCKET(p,n) p[n].second
    #define GET_PKV(p,n)     p[n].first
    #define NEW_KVALUE(key, value, bucket) new(_pairs + bucket) PairT(std::pair<KeyT, ValueT>(key, value), bucket); _num_filled ++
#else
    #define GET_KEY(p,n)     p[n].first
    #define GET_VAL(p,n)     p[n].second
    #define NEXT_BUCKET(p,n) p[n].bucket
    #define GET_PKV(p,n)     p[n]
    #define NEW_KVALUE(key, value, bucket) new(_pairs + bucket) PairT(key, value, bucket), _num_filled ++; SET_BIT(bucket)
#endif

#define MASK_BIT         32
#define MASK_N(n)        1 << (n % MASK_BIT)
#define SET_BIT(bucket) _bitmask[bucket / MASK_BIT] &= ~(MASK_N(bucket))
#define CLS_BIT(bucket) _bitmask[bucket / MASK_BIT] |= MASK_N(bucket)
#define IS_SET(bucket)  _bitmask[bucket / MASK_BIT] & (MASK_N(bucket))

#if _WIN32 || _MSC_VER > 1400
    #include <intrin.h>
#endif

namespace emhash7 {

constexpr uint32_t INACTIVE = 0xFFFFFFFF;
inline static uint32_t CTZ64(const uint64_t n)
{
#if _MSC_VER > 1400 || _WIN32
    unsigned long index;
#if __x86_64__ || __amd64__ || _M_X64
    _BitScanForward64(&index, n);
#else
    _BitScanForward(&index, n);
#endif
#elif __GNUC__ || __clang__
    uint32_t index = __builtin_ctzll(n);
#elif 1
    uint64_t index;
#if __GNUC__ || __clang__
    __asm__("bsfq %1, %0\n" : "=r" (index) : "rm" (n) : "cc");
#else
    __asm
    {
        bsfq eax, n
        mov index, eax
    }
#endif
#endif

    return (uint32_t)index;
}

template <typename First, typename Second>
struct entry {
    entry(const First& key, const Second& value, uint32_t ibucket)
        :second(value),first(key)
    {
        bucket = ibucket;
    }

    entry(First&& key, Second&& value, uint32_t ibucket)
        :second(std::move(value)), first(std::move(key))
    {
        bucket = ibucket;
    }

    template<typename K, typename V>
    entry(K&& key, V&& value, uint32_t ibucket)
        :second(std::forward<V>(value)), first(std::forward<K>(key))
    {
        bucket = ibucket;
    }

    entry(const std::pair<First,Second>& pair)
        :second(pair.second),first(pair.first)
    {
        bucket = INACTIVE;
    }

    entry(std::pair<First, Second>&& pair)
        :second(std::move(pair.second)),first(std::move(pair.first))
    {
        bucket = INACTIVE;
    }

    entry(const entry& pairT)
        :second(pairT.second),first(pairT.first)
    {
        bucket = pairT.bucket;
    }

    entry(entry&& pairT)
        :second(std::move(pairT.second)),first(std::move(pairT.first))
    {
        bucket = pairT.bucket;
    }

    entry& operator = (entry&& pairT)
    {
        second = std::move(pairT.second);
        bucket = pairT.bucket;
        first = std::move(pairT.first);
        return *this;
    }

    entry& operator = (entry& o)
    {
        second = o.second;
        bucket = o.bucket;
        first  = o.first;
        return *this;
    }

    void swap(entry<First, Second>& o)
    {
        std::swap(second, o.second);
        std::swap(first, o.first);
    }

    Second second;//int
    uint32_t bucket;
    First first; //long
};// __attribute__ ((packed));

/// A cache-friendly hash table with open addressing, linear/qua probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap
{
private:
    typedef HashMap<KeyT, ValueT, HashT, EqT> htype;

#if EMHASH_BUCKET_INDEX == 0
    typedef std::pair<KeyT, ValueT>         value_pair;
    typedef std::pair<uint32_t, value_pair> PairT;
#elif EMHASH_BUCKET_INDEX == 2
    typedef std::pair<KeyT, ValueT>         value_pair;
    typedef std::pair<value_pair, uint32_t> PairT;
#else
    typedef entry<KeyT, ValueT>             value_pair;
    typedef entry<KeyT, ValueT>             PairT;
#endif

public:
    typedef KeyT   key_type;
    typedef ValueT mapped_type;

    typedef  uint32_t       size_type;
    typedef  std::pair<KeyT,ValueT>        value_type;
    typedef  PairT&       reference;
    typedef  const PairT& const_reference;

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef value_pair*               pointer;
        typedef value_pair&               reference;

        iterator() { }
        iterator(htype* hash_map, uint32_t bucket) : _map(hash_map), _bucket(bucket) { }

        iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
            auto old_index = _bucket;
            goto_next_element();
            return {_map, old_index};
        }

        reference operator*() const
        {
            return _map->GET_PKV(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_map->GET_PKV(_pairs, _bucket));
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
            auto _bitmask = _map->_bitmask;
            do {
                _bucket++;
            } while (IS_SET(_bucket));
        }

    public:
        htype* _map;
        uint32_t  _bucket;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef value_pair*               pointer;
        typedef value_pair&               reference;

        const_iterator() { }
        const_iterator(const iterator& proto) : _map(proto._map), _bucket(proto._bucket) { }
        const_iterator(const htype* hash_map, uint32_t bucket) : _map(hash_map), _bucket(bucket) { }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            auto old_index = _bucket;
            goto_next_element();
            return {_map, old_index};
        }

        reference operator*() const
        {
            return _map->GET_PKV(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_map->GET_PKV(_pairs, _bucket));
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
            auto _bitmask = _map->_bitmask;
            do {
                _bucket++;
            } while (IS_SET(_bucket));
        }

    public:
        const htype* _map;
        uint32_t  _bucket;
    };

    void init(uint32_t bucket)
    {
        _num_buckets = 0;
        _last = 0;
        _mask = 0;
        _pairs = nullptr;
        _bitmask = nullptr;
        _num_filled = 0;
        max_load_factor(0.95f);
        reserve(bucket);
    }

    HashMap(uint32_t bucket = 4)
    {
        init(bucket);
    }

    HashMap(const HashMap& other)
    {
        _pairs = (PairT*)malloc((2 + other._num_buckets) * sizeof(PairT) + other._num_buckets / 8 + sizeof(uint64_t));
        clone(other);
    }

    HashMap(HashMap&& other)
    {
//        init(0);
        _num_filled = 0;
        _pairs = nullptr;
        swap(other);
    }

    HashMap(std::initializer_list<value_type> ilist)
    {
        init((uint32_t)ilist.size());
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin)
            do_insert(begin->first, begin->second);
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this == &other)
            return *this;

        if (is_notriviall_destructable())
            clearkv();

        if (_num_buckets != other._num_buckets) {
            free(_pairs);
            _pairs = (PairT*)malloc((2 + other._num_buckets) * sizeof(PairT) + other._num_buckets / 8 + sizeof(uint64_t));
        }

        clone(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other)
    {
        swap(other);
        return *this;
    }

    ~HashMap()
    {
        if (is_notriviall_destructable())
            clearkv();
        free(_pairs);
    }

    void clone(const HashMap& other)
    {
        _hasher      = other._hasher;
        _eq          = other._eq;
        _num_buckets = other._num_buckets;
        _num_filled  = other._num_filled;
        _mask        = other._mask;
        _loadlf      = other._loadlf;
        _last        = other._last;
        _bitmask     = (uint32_t*)((char*)_pairs + ((char*)other._bitmask - (char*)other._pairs));
        auto opairs = other._pairs;

#if __cplusplus >= 201103L || _MSC_VER > 1600 || __clang__
        if (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value)
#else
        if (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value)
#endif
            memcpy(_pairs, opairs, _num_buckets * sizeof(PairT));
        else {
            for (uint32_t bucket = 0; bucket < _num_buckets; bucket++) {
                auto next_bucket = NEXT_BUCKET(_pairs, bucket) = NEXT_BUCKET(opairs, bucket);
                if (next_bucket != INACTIVE)
                    new(_pairs + bucket) PairT(opairs[bucket]);
            }
        }
        memcpy(_pairs + _num_buckets, opairs + _num_buckets, 2 * sizeof(PairT) + _num_buckets / 8 + sizeof(uint64_t));
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher, other._hasher);
        std::swap(_eq, other._eq);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
        std::swap(_loadlf, other._loadlf);
        std::swap(_last, other._last);
        std::swap(_bitmask, other._bitmask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        uint32_t bucket = 0;
        while (IS_SET(bucket)) {
            ++bucket;
        }
        return {this, bucket};
    }

    const_iterator cbegin() const
    {
        uint32_t bucket = 0;
        while (IS_SET(bucket)) {
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
        return {this, _num_buckets};
    }

    const_iterator cend() const
    {
        return {this, _num_buckets};
    }

    const_iterator end() const
    {
        return {this, _num_buckets};
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

    HashT& hash_function() const
    {
        return _hasher;
    }

    EqT& key_eq() const
    {
        return _eq;
    }

    constexpr float max_load_factor() const
    {
        return (1 << 17) / (float)_loadlf;
    }

    void max_load_factor(float value)
    {
        if (value < 0.995f && value > 0.2f)
            _loadlf = (uint32_t)((1 << 17) / value);
    }

    constexpr size_type max_size() const
    {
        return (1 << 31) / sizeof(PairT);
    }

    constexpr size_type max_bucket_count() const
    {
        return (1 << 31) / sizeof(PairT);
    }

#ifdef EMHASH_STATIS
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
    size_type bucket_size(const uint32_t bucket) const
    {
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return 0;

        next_bucket = hash_bucket(GET_KEY(_pairs, bucket));
        uint32_t bucket_size = 1;

        //iterator each item in current main bucket
        while (true) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                break;
            }
            bucket_size++;
            next_bucket = nbucket;
        }
        return bucket_size;
    }

    size_type get_main_bucket(const uint32_t bucket) const
    {
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto& bucket_key = GET_KEY(_pairs, bucket);
        const auto main_bucket = hash_bucket(bucket_key);
        return main_bucket;
    }

    size_type get_diss(uint32_t bucket, uint32_t next_bucket) const
    {
        auto pbucket = reinterpret_cast<uint64_t>(&_pairs[bucket]);
        auto pnext   = reinterpret_cast<uint64_t>(&_pairs[next_bucket]);
        if (pbucket / EMHASH_CACHE_LINE_SIZE == pnext / EMHASH_CACHE_LINE_SIZE)
            return 0;
        uint32_t diff = pbucket > pnext ? (pbucket - pnext) : (pnext - pbucket);
        if (diff / EMHASH_CACHE_LINE_SIZE < 127)
            return diff / EMHASH_CACHE_LINE_SIZE + 1;
        return 127;
    }

    int get_bucket_info(const uint32_t bucket, uint32_t steps[], const uint32_t slots) const
    {
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return -1;

        if (hash_bucket(GET_KEY(_pairs, bucket)) != bucket)
            return 0;
        else if (next_bucket == bucket)
            return 1;

        steps[get_diss(bucket, next_bucket) % slots] ++;
        uint32_t bucket_size = 2;
        while (true) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            else if (nbucket == INACTIVE)
                break;

            steps[get_diss(nbucket, next_bucket) % slots] ++;
            bucket_size ++;
            next_bucket = nbucket;
        }
        return bucket_size;
    }

    void dump_statis(bool show_cache) const
    {
        uint32_t buckets[256] = {0};
        uint32_t steps[256]   = {0};
        char buff[1024 * 8];
        for (uint32_t bucket = 0; bucket < _num_buckets; ++bucket) {
            auto bsize = get_bucket_info(bucket, steps, 128);
            if (bsize >= 0)
                buckets[bsize] ++;
        }

        uint32_t sumb = 0, sums = 0, sumn = 0;
        uint32_t miss = 0, finds = 0, bucket_coll = 0;
        double lf = load_factor(), fk = 1.0 / pow(2.71828, lf), sum_poisson = 0;
        int bsize = sprintf (buff, "============== buckets size ration ========\n");

        miss += _num_buckets - _num_filled;
        for (uint32_t i = 1, factorial = 1; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
            double poisson = fk / factorial; factorial *= i; fk *= lf;
            sum_poisson += poisson * 100.0 * (i - 1) / i;

            const auto bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;

            sumb += bucketsi;
            sumn += bucketsi * i;
            bucket_coll += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            miss  += bucketsi * i * i;
            //(exp(-lf) * pow(lf, k)/factorial(k))
            bsize += sprintf(buff + bsize, "  %2u  %8u  %0.8lf|%0.8lf  %2.3lf\n", i, bucketsi, bucketsi * 1.0 * i / _num_filled, poisson, sumn * 100.0 / _num_filled);
            if (sumn >= _num_filled)
                break;
        }

        bsize += sprintf(buff + bsize, "========== collision miss ration ===========\n");
        for (uint32_t i = 0; show_cache && i < sizeof(steps) / sizeof(steps[0]); i++) {
            sums += steps[i];
            if (steps[i] <= 2)
                continue;
            bsize += sprintf(buff + bsize, "  %2u  %8u  %0.2lf  %.2lf\n", i, steps[i], steps[i] * 100.0 / bucket_coll, sums * 100.0 / bucket_coll);
        }

        if (sumb == 0)  return;

        bsize += sprintf(buff + bsize, "  _num_filled aver_size k.v size_kv = %u, %.2lf, %s.%s %zd\n", 
                _num_filled, _num_filled * 1.0 / sumb, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(PairT));
        bsize += sprintf(buff + bsize, "  collision,possion,cache_miss hit_find|hit_miss, load_factor = %.2lf%%,%.2lf%%,%.2lf%%  %.2lf|%.2lf, %.2lf\n",
                 (bucket_coll * 100.0 / _num_filled), sum_poisson, (bucket_coll - steps[0]) * 100.0 / _num_filled, finds * 1.0 / _num_filled, miss * 1.0 / _num_buckets, _num_filled * 1.0 / _num_buckets);

        assert(sums == bucket_coll || !show_cache);
        assert(bucket_coll == buckets[0]);
        assert(sumn == _num_filled);

        bsize += sprintf(buff + bsize, "============== buckets size end =============\n");
        buff[bsize + 1] = 0;

#if EMHASH_TAF_LOG
        FDLOG() << __FUNCTION__ << "|" << buff << endl;
#else
        puts(buff);
#endif
    }
#endif

    // ------------------------------------------------------------

    iterator find(const KeyT& key) noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    const_iterator find(const KeyT& key) const noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    bool contains(const KeyT& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    size_type count(const KeyT& key) const noexcept
    {
        return (size_type)(find_filled_bucket(key) != _num_buckets);
    }

    std::pair<iterator, iterator> equal_range(const KeyT& key) noexcept
    {
        const auto found = find(key);
        if (found == end())
            return { found, found };
        else
            return { found, std::next(found) };
    }

    /// Returns false if key isn't found.
    bool try_get(const KeyT& key, ValueT& val) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        const auto found = bucket != _num_buckets;
        if (found) {
            val = GET_VAL(_pairs, bucket);
        }
        return found;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    ValueT* try_get(const KeyT& key) noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &GET_VAL(_pairs, bucket);
    }

    /// Const version of the above
    ValueT* try_get(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &GET_VAL(_pairs, bucket);
    }

    /// Convenience function.
    ValueT get_or_return_default(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? ValueT() : GET_VAL(_pairs, bucket);
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        return do_insert(std::move(key), std::move(value));
    }

    std::pair<iterator, bool> insert(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        return do_insert(key, value);
    }

    std::pair<iterator, bool> insert(const KeyT& key, ValueT&& value)
    {
        check_expand_need();
        return do_insert(key, std::move(value));
    }

    std::pair<iterator, bool> insert(KeyT&& key, const ValueT& value)
    {
        check_expand_need();
        return do_insert(std::move(key), value);
    }

    template<typename K, typename V>
    inline std::pair<iterator, bool> do_insert(K&& key, V&& value)
    {
        const auto bucket = find_or_allocate(key);
        const auto found = NEXT_BUCKET(_pairs, bucket) == INACTIVE;
        if (found) {
            NEW_KVALUE(std::forward<K>(key), std::forward<V>(value), bucket);
        }
        return { {this, bucket}, found };
    }

    std::pair<iterator, bool> insert(const std::pair<KeyT, ValueT>& p)
    {
        check_expand_need();
        return do_insert(p.first, p.second);
    }

    std::pair<iterator, bool> insert(std::pair<KeyT, ValueT>&& p)
    {
        check_expand_need();
        return do_insert(std::move(p.first), std::move(p.second));
    }

    void insert(std::initializer_list<value_type> ilist)
    {
        reserve(ilist.size() + _num_filled);
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin)
            do_insert(begin->first, begin->second);
    }

#if 0
    template <typename Iter>
    void insert(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            emplace(*begin);
        }
    }

    template <typename Iter>
    void insert2(Iter begin, Iter end)
    {
        Iter citbeg = begin;
        Iter citend = begin;
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            if (try_insert_mainbucket(begin->first, begin->second) == INACTIVE) {
                std::swap(*begin, *citend++);
            }
        }

        for (; citbeg != citend; ++citbeg)
            insert(*citbeg);
    }
#endif

    template <typename Iter>
    void insert_unique(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin)
            do_insert_unqiue(*begin);
    }

    /// Same as above, but contains(key) MUST be false
    uint32_t insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        return do_insert_unqiue(std::move(key), std::move(value));
    }

    uint32_t insert_unique(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        return do_insert_unqiue(key, value);
    }

    uint32_t insert_unique(std::pair<KeyT, ValueT>&& p)
    {
        check_expand_need();
        return do_insert_unqiue(std::move(p.first), std::move(p.second));
    }

    uint32_t insert_unique(const std::pair<KeyT, ValueT>& p)
    {
        check_expand_need();
        return do_insert_unqiue(p.first, p.second);
    }

    template<typename K, typename V>
    inline uint32_t do_insert_unqiue(K&& key, V&& value)
    {
        auto bucket = find_unique_bucket(key);
        NEW_KVALUE(std::forward<K>(key), std::forward<V>(value), bucket);
        return bucket;
    }

#if 1
    template <class... Args>
    inline std::pair<iterator, bool> emplace(Args&&... args)
    {
        return insert(std::forward<Args>(args)...);
    }
#else
    template <class Key, class Val>
    inline std::pair<iterator, bool> emplace(Key&& key, Val&& value)
    {
        return insert(std::move(key), std::move(value));
    }

    template <class Key, class Val>
    inline std::pair<iterator, bool> emplace(const Key& key, const Val& value)
    {
        return insert(key, value);
    }
#endif

    //no any optimize for position
    template <class... Args>
    iterator emplace_hint(const_iterator position, Args&&... args)
    {
        return insert(std::forward<Args>(args)...).first;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args)
    {
        return insert(k, std::forward<Args>(args)...).first;
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }

    uint32_t try_insert_mainbucket(const KeyT& key, const ValueT& value)
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket != INACTIVE)
            return INACTIVE;

        NEW_KVALUE(key, value, bucket);
        return bucket;
    }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);

        // Check if inserting a new value rather than overwriting an old entry
        if (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
            NEW_KVALUE(key, value, bucket);
            return ValueT();
        } else {
            ValueT old_value(value);
            std::swap(GET_VAL(_pairs, bucket), old_value);
            return old_value;
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        auto bucket = find_or_allocate(key);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
            if (EMHASH_UNLIKELY(check_expand_need()))
                bucket = find_unique_bucket(key);

            NEW_KVALUE(key, std::move(ValueT()), bucket);
        }

        return GET_VAL(_pairs, bucket);
    }

    ValueT& operator[](KeyT&& key)
    {
        auto bucket = find_or_allocate(key);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (NEXT_BUCKET(_pairs, bucket) == INACTIVE) {
            if (EMHASH_UNLIKELY(check_expand_need()))
                bucket = find_unique_bucket(key);

            NEW_KVALUE(std::move(key), std::move(ValueT()), bucket);
        }

        return GET_VAL(_pairs, bucket);
    }

    // -------------------------------------------------------
    /// Erase an element from the hash table.
    /// return 0 if element was not found
    size_type erase(const KeyT& key)
    {
        const auto bucket = erase_key(key);
        if (bucket == INACTIVE)
            return 0;

        clear_bucket(bucket);
        return 1;
    }

    //iterator erase(const_iterator begin_it, const_iterator end_it)
    iterator erase(const_iterator cit)
    {
        iterator it(this, cit._bucket);
        return erase(it);
    }

    /// Erase an element typedef an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
        //erase from main bucket, return main bucket as next
        return (bucket == it._bucket) ? ++it : it;
    }

     /// No need return next iterator for performace issuse in some case
    void _erase(const_iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
    }

    static constexpr bool is_notriviall_destructable()
    {
#if __cplusplus >= 201103L || _MSC_VER > 1600 || __clang__
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return !(std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    static constexpr bool is_copy_trivially()
    {
#if __cplusplus >= 201103L || _MSC_VER > 1600 || __clang__
        return !(std::is_trivially_copy_assignable<KeyT>::value && std::is_trivially_copy_assignable<ValueT>::value);
#else
        return !(std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    void clearkv()
    {
        for (uint32_t bucket = 0; _num_filled > 0; ++bucket) {
            if (NEXT_BUCKET(_pairs, bucket) != INACTIVE)
                clear_bucket(bucket);
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (is_notriviall_destructable() || sizeof(PairT) > EMHASH_CACHE_LINE_SIZE / 2 || _num_filled < _num_buckets / 4)
            clearkv();
        else {
            memset(_pairs, 0xFFFFFFFF, sizeof(_pairs[0]) * _num_buckets);
            memset(_bitmask, 0xFFFFFFFF, _num_buckets / 8);
            _bitmask[_num_buckets / MASK_BIT] &= (1 << _num_buckets % MASK_BIT) - 1;
        }
        _num_filled = 0;
        _last = 0;
    }

    void shrink_to_fit()
    {
        rehash(_num_filled);
    }

    /// Make room for this many elements
    bool reserve(uint64_t num_elems)
    {
        const auto required_buckets = (uint32_t)(num_elems * _loadlf >> 17);
        if (EMHASH_LIKELY(required_buckets < _mask))
            return false;

    #ifdef EMHASH_STATIS
        //if (_num_filled > 1000'000) dump_statis(0);
    #endif
        rehash(required_buckets + 2);
    #ifdef EMHASH_STATIS
        if (_num_filled > 1000'000) dump_statis(1);
    #endif
        return true;
    }

private:
    void rehash(uint32_t required_buckets)
    {
        if (required_buckets < _num_filled)
            return;

        uint32_t num_buckets = _num_filled > 65536 ? (1u << 16) : 4u;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        const auto num_byte = num_buckets / 8;
        auto new_pairs = (PairT*)malloc((2 + num_buckets) * sizeof(PairT) + num_byte + sizeof(uint64_t));
        auto old_num_filled  = _num_filled;
        auto old_pairs = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _last = 0;
        _bitmask     = (uint32_t*)(new_pairs + 2 + num_buckets);

        if (sizeof(PairT) <= EMHASH_CACHE_LINE_SIZE / 2)
            memset(new_pairs, INACTIVE, sizeof(_pairs[0]) * num_buckets);
        else
            for (uint32_t bucket = 0; bucket < num_buckets; bucket++)
                NEXT_BUCKET(new_pairs, bucket) = INACTIVE;

        /***************** ----------------------**/
        memset(_bitmask, INACTIVE, num_byte);
        memset(((char*)_bitmask) + num_byte, 0, sizeof(uint64_t));
        _bitmask[num_buckets / MASK_BIT] &= (1 << num_buckets % MASK_BIT) - 1;
        /***************** ----------------------**/
        memset(new_pairs + _num_buckets, 0, sizeof(PairT) * 2);
        _pairs       = new_pairs;

        for (uint32_t src_bucket = 0; _num_filled < old_num_filled; src_bucket++) {
            if (NEXT_BUCKET(old_pairs, src_bucket) == INACTIVE)
                continue;

            auto&& key = GET_KEY(old_pairs, src_bucket);
            const auto bucket = find_unique_bucket(key);
            NEW_KVALUE(std::move(key), std::move(GET_VAL(old_pairs, src_bucket)), bucket);
            old_pairs[src_bucket].~PairT();
        }

#if EMHASH_REHASH_LOG || EMHASH_TAF_LOG
        if (_num_filled > EMHASH_REHASH_LOG) {
            auto mbucket = _num_filled;
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/aver_size/K.V/pack/ = %u/%2.lf/%s.%s/%zd",
                    _num_filled, double (_num_filled) / mbucket, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(_pairs[0]));
#if EMHASH_TAF_LOG
            static uint32_t ihashs = 0;
            FDLOG() << "hash_nums = " << ihashs ++ << "|" <<__FUNCTION__ << "|" << buff << endl;
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

    void clear_bucket(uint32_t bucket)
    {
        NEXT_BUCKET(_pairs, bucket) = INACTIVE;
        _pairs[bucket].~PairT();
        _num_filled --;
        CLS_BIT(bucket);
    }

#if 1
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
            if (is_copy_trivially())
                GET_PKV(_pairs, bucket).swap(GET_PKV(_pairs, next_bucket));
            else
                GET_PKV(_pairs, bucket) = GET_PKV(_pairs, next_bucket);

            NEXT_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            return next_bucket;
        }/* else if (EMHASH_UNLIKELY(bucket != hash_bucket(GET_KEY(_pairs, bucket))))
            return INACTIVE;
        */

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
#else
    uint32_t erase_key(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto eqkey = _eq(key, GET_KEY(_pairs, bucket));
        if (next_bucket == bucket)
            return eqkey ? bucket : INACTIVE;
        else if (bucket != hash_bucket(GET_KEY(_pairs, bucket)))
            return INACTIVE;

        //find erase key and swap to last bucket
        uint32_t prev_bucket = bucket, find_bucket = INACTIVE;
        next_bucket = bucket;
        while (true) {
            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (_eq(key, GET_KEY(_pairs, next_bucket))) {
                find_bucket = next_bucket;
                if (nbucket == next_bucket) {
                    NEXT_BUCKET(_pairs, prev_bucket) = prev_bucket;
                    break;
                }
            }
            if (nbucket == next_bucket) {
                if (find_bucket != INACTIVE) {
                    GET_PKV(_pairs, find_bucket).swap(GET_PKV(_pairs, nbucket));
//                    GET_PKV(_pairs, find_bucket) = GET_PKV(_pairs, nbucket);
                    NEXT_BUCKET(_pairs, prev_bucket) = prev_bucket;
                    find_bucket = nbucket;
                }
                break;
            }
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return find_bucket;
    }
#endif

    uint32_t erase_bucket(const uint32_t bucket)
    {
        const auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        const auto main_bucket = hash_bucket(GET_KEY(_pairs, bucket));
        if (bucket == main_bucket) {
            if (bucket != next_bucket) {
                const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
                if (is_copy_trivially())
                    GET_PKV(_pairs, bucket).swap(GET_PKV(_pairs, next_bucket));
                else
                    GET_PKV(_pairs, bucket) = GET_PKV(_pairs, next_bucket);
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
        if (next_bucket == INACTIVE)
            return _num_buckets;
#if 1
        if (_eq(key, GET_KEY(_pairs, bucket)))
            return bucket;
        else if (next_bucket == bucket)
            return _num_buckets;
#elif 0
        else if (_eq(key, GET_KEY(_pairs, bucket)))
            return bucket;
        else if (next_bucket == bucket)
            return _num_buckets;

        else if (_eq(key, GET_KEY(_pairs, next_bucket)))
            return next_bucket;
        const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
        if (nbucket == next_bucket)
            return _num_buckets;
        next_bucket = nbucket;
#elif 0
        const auto bucket = hash_bucket(key);
        if (IS_SET(bucket))
            return _num_buckets;
        else if (_eq(key, GET_KEY(_pairs, bucket)))
            return bucket;

        auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        if (next_bucket == bucket)
            return _num_buckets;
#endif
//        else if (bucket != hash_bucket(bucket_key))
//            return _num_buckets;

        while (true) {
            if (_eq(key, GET_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = NEXT_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        return _num_buckets;
    }

    //kick out bucket and find empty to occpuy
    //it will break the orgin link and relnik again.
    //before: main_bucket-->prev_bucket --> bucket   --> next_bucket
    //atfer : main_bucket-->prev_bucket --> (removed)--> new_bucket--> next_bucket
    uint32_t kickout_bucket(const uint32_t main_bucket, const uint32_t bucket)
    {
        const auto next_bucket = NEXT_BUCKET(_pairs, bucket);
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        NEXT_BUCKET(_pairs, prev_bucket) = new_bucket;
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket]));
        SET_BIT(new_bucket);
        if (next_bucket == bucket)
            NEXT_BUCKET(_pairs, new_bucket) = new_bucket;
        _pairs[bucket].~PairT(); NEXT_BUCKET(_pairs, bucket) = INACTIVE;
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

#if EMHASH_LRU_SET
        auto prev_bucket = bucket;
#endif
        //find next linked bucket and check key
        while (true) {
            if (_eq(key, GET_KEY(_pairs, next_bucket))) {
#if EMHASH_LRU_SET
                GET_PKV(_pairs, next_bucket).swap(GET_PKV(_pairs, prev_bucket));
                return prev_bucket;
#else
                return next_bucket;
#endif
            }

#if EMHASH_LRU_SET
            prev_bucket = next_bucket;
#endif

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
    uint32_t find_empty_bucket(const uint32_t bucket_from)
    {
        const auto bucket1 = bucket_from + 1;
        if (NEXT_BUCKET(_pairs, bucket1) == INACTIVE)
            return bucket1;

#if 0
        const auto bucket2 = bucket_from + 2;
        if (NEXT_BUCKET(_pairs, bucket2) == INACTIVE)
            return bucket2;
#endif

        //fast find by bit
        const auto boset = bucket_from % 8;
        const auto bmask = *(uint64_t*)((unsigned char*)_bitmask + bucket_from / 8) >> boset;
        if (bmask != 0)
            return bucket_from + CTZ64(bmask) - 0;

#ifndef QC
        const auto bmask2 = *((uint64_t*)_bitmask + _last);
        if (bmask2 != 0)
            return _last * 64 + CTZ64(bmask2);
#endif

        const auto qmask = (64 + _num_buckets - 1) / 64 - 1;
//        for (uint32_t last = 3, step = (bucket_from + _num_filled) & qmask; ;step = (step + ++last) & qmask) {
        for (uint32_t last = 3, step = (bucket_from + 2 * 64) & qmask; ;step = (step + ++last) & qmask) {
//        for (uint32_t step = _last + 1; ; step = ++step & qmask) {
            const auto next2 = step;
            const auto bmask2 = *((uint64_t*)_bitmask + next2);
            if (bmask2 != 0) {
#ifdef QC
                _last = next2;
#endif
                return next2 * 64 + CTZ64(bmask2);
            }
        }

        return 0;
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

    //the first cache line packed
    inline uint32_t hash_bucket(const KeyT& key) const
    {
        return _hasher(key) & _mask;
    }

private:
    HashT     _hasher;
    EqT       _eq;
    uint32_t  _mask;
    uint32_t  _num_buckets;

    uint32_t  _num_filled;
    uint32_t  _last;
    uint32_t  _loadlf;
    PairT*    _pairs;
    uint32_t* _bitmask;
};
} // namespace emhash
#if __cplusplus > 199711
//template <class Key, class Val> using emihash = emhash1::HashMap<Key, Val, std::hash<Key>>;
#endif
