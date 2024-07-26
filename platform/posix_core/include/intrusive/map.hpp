/*
 * Copyright (C) 2022-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <set>

// A wrapper around [std::set] providing (key,value)-based API. [V] should
// inherit from [MapKey<K>]. When reinserting the *same* value, the key is
// overwritten

// That is,
// map.insert(10, &my_elem);
// map.insert(5, &my_elem);
// will result in the mapping from 10 to my_elem being removed and replaced by
// the mapping from 5 to my_elem.

template<typename K>
class MapKey {

public:
    K _key;
};

template<typename K, typename V>
class MapKV {

private:
    std::set<V *> _map;

    class iterator {
    private:
        typename std::set<V *>::iterator _it;

    public:
        explicit iterator(typename std::set<V *>::iterator it) : _it(it) {}

        iterator &operator++() {
            _it++;
            return *this;
        }

        bool operator==(const iterator &other) { return this->_it == other._it; }
        bool operator!=(const iterator &other) { return this->_it != other._it; }

        V &operator*() { return **_it; }
        V *operator->() { return *_it; }
    };

public:
    V *insert(K key, V *value) {
        static_cast<MapKey<K> *>(value)->_key = key;

        V *old = nullptr;
        auto it = _map.find(value);

        if (it != _map.end()) {
            _map.erase(it);
            old = *it;
        }
        _map.insert(value);
        return old;
    }

    void remove_existing(V *to_be_removed) {
        _map.erase(to_be_removed);
        return;
    }

    using iterator = iterator; // typename std::set<V>::iterator;

    iterator begin() { return iterator(_map.begin()); }
    iterator end() { return iterator(_map.end()); }

    V *operator[](const K &key) const {

        for (auto it = _map.begin(); it != _map.end(); ++it) {

            if ((static_cast<MapKey<K> *>(*it))->_key == key)
                return *it;
        }

        return nullptr;
    }
};
