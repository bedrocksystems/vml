/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file
 *  \brief Exposes a map that links a Range to a custom object
 */

#include <cstddef>
#include <set>

/*! \brief Represent a mathematical range
 */
template<typename T>
class Range {
public:
    Range(T begin = T(0), size_t size = size_t(0)) : _begin(begin), _size(size) {}

    constexpr inline T begin() const { return _begin; }
    constexpr inline T end() const { return begin() + size(); }
    constexpr inline T last() const { return empty() ? begin() : begin() + size() - 1; }
    constexpr inline size_t size() const { return _size; }
    constexpr inline bool empty() const { return _size == 0; }

    constexpr inline bool intersect(Range<T> &r) const {
        return !empty() && !r.empty() && r.begin() < end() && begin() < r.end();
    }
    constexpr inline bool contains(T val) const {
        return empty() ? false : val >= begin() && val < end();
    }
    constexpr inline bool contains(const Range<T> &r) const {
        return (!r.empty()) && contains(r.begin()) && contains(r.last());
    }

    bool operator<(const Range<T> &r) const { return end() <= r.begin(); }

private:
    T _begin;
    size_t _size;
};

/*! \brief Item that will be stored in the RangeMap
 */
template<typename T>
class RangeNode : public Range<T> {
public:
    RangeNode(const Range<T> &r) : Range<T>(r.begin(), r.size()) {}
};

/*! \brief Custom comparator for std::set
 *
 *  We want this comparator to declare two ranges that intersect as
 *  equivalent because we do not want any overlap in the RangeMap.
 */
template<typename T>
class Range_compare {
public:
    constexpr bool operator()(const T &a, const T &b) const {
        if (a->intersect(*b))
            return false;

        return *a < *b;
    }
};

/*! \brief Efficiently store a set of non-overlapping RangeNodes
 */
template<typename T>
class RangeMap {
public:
    /*! \brief Insert a range node in the map
     *  \param entry the range node to add
     *  \return true if the element was inserted (meaning it didn't overlap
     * with any other element), false otherwise.
     */
    bool insert(RangeNode<T> *entry) { return _set.insert(entry).second; }

    /*! \brief Find an element that overlaps with the given Range
     *  \param r range that should overlap with the RangeNode
     *  \return the element that overlaps with r. Otherwise, nullptr.
     */
    RangeNode<T> *lookup(Range<T> *r) const {
        RangeNode<T> item(*r);
        auto res = _set.find(&item);

        if (res != _set.end())
            return *res;
        else
            return nullptr;
    }

    /*! \brief Apply f to all elements of the map
     *  \param f function to call on all elements
     */
    template<typename U, typename TARG>
    void iter(void (*f)(U *, TARG), TARG arg) {
        for (auto *r : _set)
            f(static_cast<U *>(r), arg);
    }

    RangeNode<T> *remove(Range<T> r) {
        RangeNode<T> item(r);
        auto res = _set.find(&item);

        if (res != _set.end()) {
            RangeNode<T> *ret = *res;
            _set.erase(res);
            return ret;
        }

        return nullptr;
    }

private:
    std::set<RangeNode<T> *, Range_compare<RangeNode<T> *>> _set;
};
