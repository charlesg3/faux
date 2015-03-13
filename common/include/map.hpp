#pragma once

#include "circular_queue.hpp"

// A very stupid map implementation for low memory footprint and
// locality. Assumes that few elements are stored at a time.

template<class Key, class Value>
struct Entry {
    Key key;
    Value value;
};

template <class Key, class Value, size_t CAPACITY>
class Map : public CircularQueue< Entry<Key, Value>, CAPACITY > {
public:
    typedef CircularQueue< Entry<Key, Value>, CAPACITY> Parent;

    typedef typename Parent::Iterator Iterator;
    using Parent::at;
    using Parent::size;
    using Parent::end;

    Map() {}
    ~Map() {}

    void insert(Key key, Value value) {
        Entry<Key, Value> e = { key, value };
        this->push(e);
    };

    Iterator find(Key key) {
        for (size_t i = 0; i < CircularQueue<Entry<Key,Value>,CAPACITY>::size(); i++) {
            if (at(i).key == key) {
                return &at(i);
            }
        }
        return end();
    }
};
