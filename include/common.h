//
// Created by Cshuang on 2020/10/3.
//

#ifndef ART_TEST_COMMON_H
#define ART_TEST_COMMON_H

#include <cstddef>
#include <cstdint>
#include "bucket.h"

namespace alert {

    // A CDF coordinate.
    template<typename KeyType>
    struct Coord {
        KeyType x;
        double y;
    };

    template<typename KeyType>
    struct SegmentMessage {
        KeyType key;
        size_t offset;
        uint32_t size;
        float slope;
//        bool full; // if could add more point in the end
    };

    struct SearchBound {
        size_t begin;
        size_t end; // Exclusive.
    };

    template<typename KeyType, typename ValueType>
    struct KeyValue {
        KeyType key;
        ValueType value;
    };



} // namespace alert

#endif //ART_TEST_COMMON_H
