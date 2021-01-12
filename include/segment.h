//
// Created by Cshuang on 2020/10/10.
//

#ifndef ART_TEST_SEGMENT_H
#define ART_TEST_SEGMENT_H

#include <cstdint>
#include <cmath>
#include <vector>
#include <cstring>
#include "common.h"
#include "bucket.h"
#include <iostream>

namespace alert {

    template<typename KeyType, typename ValueType>
    class Segment {

    public:

        typedef OverflowBuffer<KeyType, ValueType>* OverflowBufferPtr;

        Segment(): keys_(nullptr), values_(nullptr), buffers_(nullptr), /*full_(false),*/
                   num_array_keys_(0), slope_(0.0), num_buffers_keys_(0), num_buffer_sorted_keys_(0), alpha_(32), pre_(nullptr), next_(nullptr) {
            segment_allocated_byte += sizeof(*this);
        }

        ~Segment() {
            segment_allocated_byte -= sizeof(*this);
            if (keys_) {
                if (buffers_) {
                    for (uint32_t i = 0; i < num_array_keys_; i++) {
                        if (buffers_[i]) delete buffers_[i];
                    }
                    delete []buffers_;
                }
                delete []keys_;
                delete []values_;
            }
            if (pre_) {
                pre_->set_next_segment(next_);
            }
        }

        inline void AddKV(const SegmentMessage<KeyType> &seg_msg, const std::vector<KeyType> &keys, const std::vector<ValueType> &values) {
            num_array_keys_ = seg_msg.size;
            keys_ = reinterpret_cast<KeyType*>(malloc(num_array_keys_ * sizeof(KeyType)));
            values_ = reinterpret_cast<ValueType*>(malloc(num_array_keys_ * sizeof(ValueType)));
            buffers_ = reinterpret_cast<OverflowBufferPtr*>(malloc(num_array_keys_ * sizeof(OverflowBufferPtr)));

            memcpy(keys_, keys.data() + seg_msg.offset, num_array_keys_ * sizeof(KeyType));
            memcpy(values_, values.data() + seg_msg.offset, num_array_keys_ * sizeof(ValueType));
//            for (int i = 0; i < num_array_keys_; i++) buffers_[i] = new OverflowBuffer<KeyType, ValueType>;
            memset(buffers_, 0, num_array_keys_ * sizeof(OverflowBufferPtr));

        }

        inline void Insert(KeyType key, ValueType value, size_t max_error) {
            SearchBound bound = GetSearchBound(key, max_error);
            auto it = std::lower_bound(keys_ + bound.begin, keys_ + bound.end, key);
            size_t pos = it - keys_;
            auto& buffer = buffers_[pos];
            if (buffer == nullptr) buffer = new OverflowBuffer<KeyType, ValueType>;

            buffer->Insert(key, value);
            num_buffers_keys_ += 1;
        }


        inline bool Find(KeyType key, size_t max_error, ValueType& value) {
            SearchBound bound = GetSearchBound(key, max_error);
            auto it = std::lower_bound(keys_ + bound.begin, keys_ + bound.end, key);
            size_t pos = it - keys_;
            if (keys_[pos] == key) {
                value = values_[pos];
                return true;
            }
            return buffers_[pos] != nullptr && buffers_[pos]->Find(key, value);
        }

        inline void Range(KeyType start_key, KeyType end_key, size_t max_error, std::vector<std::pair<KeyType, ValueType>> &kvs, bool& early_stop) {
            SearchBound bound = GetSearchBound(start_key, max_error);
            auto it = std::lower_bound(keys_ + bound.begin, keys_ + bound.end, start_key);
            size_t pos = it - keys_;
            for ( ; pos != num_array_keys_ && keys_[pos] < end_key; ++pos) {
                if (__glibc_unlikely(buffers_[pos] != nullptr)) {
                    buffers_[pos]->Range(start_key, end_key, kvs, num_buffer_sorted_keys_);
                }
                kvs.emplace_back(keys_[pos], values_[pos]);
            }
            if (__glibc_likely(pos < num_array_keys_)) early_stop = true;
        }

        inline void ToSortedData(std::vector<KeyType>& keys, std::vector<ValueType>& values) {
            for (size_t i = 0; i < num_array_keys_; ++i) {
                if (buffers_[i]) {
                    buffers_[i]->ToSortedData(keys, values);
                }
                keys.push_back(keys_[i]);
                values.push_back(values_[i]);
            }
        }

        inline void set_slope(float slope) { slope_ = slope; }

        inline void set_pre_segment(Segment<KeyType, ValueType> *pre) {
            pre_ = pre;
        }

        inline void set_next_segment(Segment<KeyType, ValueType> *next) {
            next_ = next;
        }

//        void set_full(bool full) { full_ = full; }
//
//        bool full() { return full_; }


        inline Segment<KeyType, ValueType> * pre_segment() {
            return pre_;
        }

        inline Segment<KeyType, ValueType> * next_segment() {
            return next_;
        }

        inline KeyType* keys() { return keys_; }
        inline ValueType* values() { return values_; };
        inline OverflowBufferPtr* buffers() { return buffers_; }
        inline uint32_t array_size() { return num_array_keys_; }
        inline uint32_t GetTotalKvNum() {
            return num_array_keys_ + num_buffers_keys_;
        }

        inline bool IsRetain(size_t avg_num_seg_keys) {
            // lazy retrain
            // Retrain when the number of sorted keys in buffer reaches a certain threshold to reduce sorting overhead.
            if (GetTotalKvNum() > avg_num_seg_keys * alpha_ && num_buffer_sorted_keys_ * 1.0 / num_buffers_keys_ > 0.6) {
//                std::cout << alpha_ * avg_num_seg_keys << std::endl;
                alpha_ = alpha_  * 2;
                return true;
            }
            return false;
        }

        inline KeyType back() {
            return keys_[num_array_keys_ - 1];
        }

        static uint64_t segment_allocated_byte;

    private:

        // Returns a search bound [begin, end) around the estimated position.
        inline SearchBound GetSearchBound(const KeyType key, size_t max_error)  {
            size_t estimate = 0;
            if (key >= keys_[0])
                estimate = slope_ * (key - keys_[0]);
            else return {0, 0};

            size_t begin = 0, end = 0;
            // `end` is exclusive.
            if (keys_[estimate] < key) {
                begin = (estimate + 1 > num_array_keys_) ? num_array_keys_ : estimate + 1;
                end = (estimate + max_error + 1 > num_array_keys_) ? num_array_keys_ : (estimate + max_error + 1);
            } else {
                begin = (estimate < max_error) ? 0 : estimate - max_error;
                end = estimate;
            }

            return SearchBound{begin, end};
        }

        KeyType *keys_;
        ValueType *values_;
        OverflowBufferPtr  *buffers_;

        Segment<KeyType, ValueType> *pre_;
        Segment<KeyType, ValueType> *next_;

//        bool full_;
        float slope_;
        uint32_t  num_array_keys_;

        uint32_t num_buffers_keys_;
        uint32_t num_buffer_sorted_keys_;
        uint32_t alpha_;
    };
}

#endif //ART_TEST_SEGMENT_H
