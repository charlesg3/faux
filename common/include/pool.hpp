#pragma once

/* Allocate objects of a fixed size. */

template<class element_type, size_t num_elements>
class Pool {
public:
    Pool() {
        nfree = num_elements;
        for (size_t i = 0; i < num_elements; i++) {
            free_list[i] = i;
        }
    }

    static const int NUM_ELEMENTS = num_elements;
    typedef element_type Element;

    Element * allocate() {
        if (nfree == 0) {
            return NULL;
        } else {
            size_t index = free_list[--nfree];
            return &heap[index];
        }
    }

    void free(Element * ptr) {
        size_t index = ptr - heap;
        free_list[nfree++] = index;
    }

    size_t available() {
        return nfree;
    }

private:
    Element heap[NUM_ELEMENTS];
    size_t free_list[NUM_ELEMENTS];
    size_t nfree;
};
