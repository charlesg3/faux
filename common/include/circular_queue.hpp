#pragma once

template<class T, size_t CAPACITY>
class CircularQueue {
public:
    CircularQueue()
        : _at(0)
        , _size(0)
    {}

    bool push(T t) {
        if (_size == CAPACITY) {
            return false;
        }

        size_t tail = _at + _size;
        tail %= CAPACITY;
        _data[tail] = t;

        ++_size;
        return true;
    }

    void pop() {
        ++_at;
        _at %= CAPACITY;
        --_size;
    }

    inline T & front() {
        return _data[_at];
    }

    size_t size() {
        return _size;
    }

    size_t capacity() {
        return CAPACITY;
    }

    typedef T * Iterator;

    Iterator begin() {
        return &front();
    }

    Iterator end() {
        return &at(size());
    }

    inline T & at(size_t offset) {
        size_t index = (_at + offset) % CAPACITY;
        return _data[index];
    }

    void rm(size_t offset) {
        size_t index = (_at + offset) % CAPACITY;
        size_t tail = (_at + _size - 1) % CAPACITY;
        _data[index] = _data[tail];
        --_size;
    }

    void rm(Iterator it) {
        size_t offset = it - &front();
        rm(offset);
    }

private:
    size_t _at;
    size_t _size;
    T _data[ CAPACITY ];
};
