//
// Created by Cshuang on 2020/10/3.
//

#ifndef ART_TEST_ALERT_INDEX_H
#define ART_TEST_ALERT_INDEX_H

#include <algorithm>
#include <cmath>
#include <iostream>

#include "builder.h"
#include "art_tree.h"
#include "segment.h"

namespace alert {

    template<typename KeyType, typename ValueType>
    class Alert {
    public:

        Alert(size_t max_error = 32, size_t overflow_threshold = 1024)
                : min_key_(std::numeric_limits<KeyType>::max()),
                  max_key_(std::numeric_limits<KeyType>::min()),
                  num_total_keys_(0),
                  num_seg_(0),
                  overflow_threshold_(overflow_threshold),
                  max_error_(max_error), segments_head_(nullptr), segments_tail_(nullptr) {
            Segment<KeyType, ValueType>::segment_allocated_byte = 0;
        }

        Alert(const Alert&) = delete ;
        Alert &operator=(const Alert&) = delete ;

        ~Alert() {
            // todo free segments
            // ...
            auto cur_seg = segments_head_;
            while  (cur_seg) {
                auto next_seg = cur_seg->next_segment();
                delete cur_seg;
                cur_seg = next_seg;
            }
            Segment<KeyType, ValueType>::segment_allocated_byte = 0;
        }

        // Keys must be sorted.
        void BulkLoad(const std::vector<KeyType> &keys, const std::vector<ValueType> &values) {
            assert(keys.size() > 0);
            assert(keys.size() == values.size());

            // Build Alert.
            min_key_ = std::min(min_key_, keys.front());
            max_key_ = std::max(max_key_, keys.back());

            alert::Builder<KeyType> asb(min_key_, max_key_, max_error_);
            for (const auto& key : keys) {
                asb.AddKey(key);
            }
            asb.Finalize();

            auto &seg_message = asb.get_segments_message();
            Segment<KeyType, ValueType> *pre_seg = nullptr;
            for (const SegmentMessage<KeyType> & msg : seg_message) {
                auto seg = new Segment<KeyType, ValueType>();
                seg->AddKV(msg, keys, values);
                seg->set_pre_segment(pre_seg);
                seg->set_slope(msg.slope);
//                seg->set_full(msg.full);
                if (pre_seg) pre_seg->set_next_segment(seg);
                else segments_head_ = seg;
                pre_seg = seg;
                tree_.Insert(msg.key, reinterpret_cast<uintptr_t>(seg));
            }
            segments_tail_ = pre_seg;
            num_seg_ += seg_message.size();
            num_total_keys_ = keys.size();
            num_seg_array_keys_ = keys.size();
//            tree_.print_node_msg();
        }

        inline void Insert(KeyType key, ValueType value) {
            num_total_keys_ += 1;
            if (segments_head_ == nullptr || key > max_key_) {
                global_overflow_buffer_.ReuseInsert(key, value );
                num_global_overflow_keys_ += 1;
                if ((num_seg_ == 0 && num_total_keys_ > overflow_threshold_) || ( num_seg_ && num_global_overflow_keys_ > num_seg_array_keys_ / num_seg_ )) {
//                    std::cout << "transform " << num_global_overflow_keys_ << std::endl;
                    TransformOverflowToSegment();
                }
                return;
            }
            auto seg = GetSplineSegment(key);
            seg->Insert(key, value, max_error_);

            if (seg->IsRetain(num_seg_array_keys_ / num_seg_)) {
//                std::cout << "retain " << num_seg_array_keys_ << " " << num_seg_ << " " <<  num_seg_array_keys_ / num_seg_ << std::endl;
                Retrain(seg);
            }
        }

        bool Find(KeyType key, ValueType& value) {
            if (__glibc_unlikely(segments_head_ == nullptr || key > max_key_)){
                return global_overflow_buffer_.Find(key, value);
            }
            return GetSplineSegment(key)->Find(key, max_error_, value);
        }

        void Range(KeyType start_key, KeyType end_key, std::vector<std::pair<KeyType, ValueType>> &kvs) {
            bool early_stop = false;
            uint32_t sorted_num = 0;
            if (__glibc_unlikely(segments_head_ == nullptr || start_key > max_key_)) {
                if (!global_overflow_buffer_.Empty())
                    global_overflow_buffer_.Range(start_key, end_key, kvs, sorted_num);
                return ;
            }
            auto seg = GetSplineSegment(start_key);
            seg->Range(start_key, end_key, max_error_, kvs, early_stop);
            while (!early_stop && (seg = seg->next_segment())) {
                seg->Range(start_key, end_key, max_error_, kvs, early_stop);
            }
            if (__glibc_unlikely(end_key > max_key_ && !global_overflow_buffer_.Empty())) {
                global_overflow_buffer_.Range(start_key, end_key, kvs, sorted_num);
            }
        }

        // Returns the size in bytes.
        size_t GetSizeInByte() const {
            return sizeof(*this) +  tree_.size() + Segment<KeyType, ValueType>::segment_allocated_byte;
        }

        size_t num_seg() {
            return num_seg_;
        }

        // Returns the spline segment that contains the `key`:
        Segment<KeyType, ValueType>* GetSplineSegment(const KeyType key) {
            return reinterpret_cast<Segment<KeyType, ValueType>*>(tree_.LowerBound(key));
        }

    private:

        void Retrain(Segment<KeyType, ValueType>* segment) {

            std::vector<KeyType> keys;
            std::vector<ValueType> values;

            Segment<KeyType, ValueType> *pre_seg = segment->pre_segment(), *next_seg = segment->next_segment();

            size_t total_kv_num = segment->GetTotalKvNum();
//            if (pre_seg  &&  !pre_seg->full()) {
//                std::cout << pre_seg->back() << std::endl;
//                total_kv_num += pre_seg->GetTotalKvNum();
//            }
//            // merge global overflow buffer.
//            if (next_seg == nullptr) {
//                total_kv_num += num_global_overflow_keys_;
//            } else if (!next_seg->full()){
//                total_kv_num += next_seg->GetTotalKvNum();
//            }
            keys.reserve(total_kv_num);
            values.reserve(total_kv_num);

//            if (pre_seg && !pre_seg->full()) {
//                pre_seg->ToSortedData(keys, values);
//                tree_.Remove(pre_seg->back());
//                auto del_seg = pre_seg;
//                pre_seg = pre_seg->pre_segment();
//                //delete del_seg;
//            }
            segment->ToSortedData(keys, values);
            tree_.Remove(segment->back());
            delete segment;
//            if (next_seg == nullptr ) {
//                if (!global_overflow_buffer_.Empty()) {
//                    global_overflow_buffer_.ToSortedData(keys, values);
//                    global_overflow_buffer_.Clear();
//                    num_global_overflow_keys_ = 0;
//                }
//            } else if (!next_seg->full()){
//                next_seg->ToSortedData(keys, values);
//                tree_.Remove(next_seg->back());
//                auto del_seg = next_seg;
//                next_seg = next_seg->pre_segment();
//            }

            alert::Builder<KeyType> asb(keys.front(), keys.back(), max_error_);
            for (const auto& key : keys) {
                asb.AddKey(key);
            }
            asb.Finalize();

            auto &seg_message = asb.get_segments_message();

//            std::cout << keys.front() <<  "---------" << keys.back() << " " << keys.size() << " " << num_seg_ <<  std::endl;
            for (const SegmentMessage<KeyType> & msg : seg_message) {
                auto seg = new Segment<KeyType, ValueType>();
                seg->AddKV(msg, keys, values);
                seg->set_pre_segment(pre_seg);
                seg->set_slope(msg.slope);
//                seg->set_full(msg.full);
//                std::cout << msg.key << " " << seg_message.size() << " "  << msg.full << " " << msg.size << std::endl;
                if (pre_seg) pre_seg->set_next_segment(seg);
                else segments_head_ = seg;
                pre_seg = seg;
                tree_.Insert(msg.key, reinterpret_cast<uintptr_t>(seg));
            }
            pre_seg->set_next_segment(next_seg);
            if (next_seg) next_seg->set_pre_segment(pre_seg);
            else segments_tail_ = pre_seg;
            num_seg_ += (seg_message.size() - 1);
            num_seg_array_keys_ += keys.size();
        }

        void TransformOverflowToSegment() {

            std::vector<KeyType> keys;
            std::vector<ValueType> values;

            size_t total_kv_num = num_global_overflow_keys_;

            Segment<KeyType, ValueType> *pre_seg = segments_tail_, *next_seg = nullptr;

            if (segments_tail_ /* && !segments_tail_->full() */) {
                total_kv_num += segments_tail_->GetTotalKvNum();
            }

            keys.reserve(total_kv_num);
            values.reserve(total_kv_num);

            if (segments_tail_ /* && !segments_tail_->full() */) {
                segments_tail_->ToSortedData(keys, values);
                tree_.Remove(segments_tail_->back());
                pre_seg = segments_tail_->pre_segment();
                delete segments_tail_;
                num_seg_ -= 1;
            }

            global_overflow_buffer_.ToSortedData(keys, values);

            alert::Builder<KeyType> asb(keys.front(), keys.back(), max_error_);
            for (const auto& key : keys) {
                asb.AddKey(key);
            }
            asb.Finalize();

            auto &seg_message = asb.get_segments_message();
            for (const SegmentMessage<KeyType> & msg : seg_message) {
                auto seg = new Segment<KeyType, ValueType>();
                seg->AddKV(msg, keys, values);
                seg->set_pre_segment(pre_seg);
                seg->set_slope(msg.slope);
//                seg->set_full(msg.full);
                if (pre_seg) pre_seg->set_next_segment(seg);
                else segments_head_ = seg;
                pre_seg = seg;
                tree_.Insert(msg.key, reinterpret_cast<uintptr_t>(seg));
            }
            pre_seg->set_next_segment(next_seg);
            if (next_seg) next_seg->set_pre_segment(pre_seg);
            else segments_tail_ = pre_seg;
            global_overflow_buffer_.Clear();
            num_global_overflow_keys_ = 0;
            num_seg_ += seg_message.size();
            num_seg_array_keys_ += keys.size();
        }

        KeyType min_key_;
        KeyType max_key_;
        size_t num_total_keys_;
        size_t num_seg_array_keys_;
        size_t num_global_overflow_keys_ = 0;
        size_t max_error_;

        size_t num_seg_;

        size_t overflow_threshold_;

        ArtTree<KeyType> tree_;


        OverflowBuffer<KeyType, ValueType> global_overflow_buffer_;

        Segment<KeyType, ValueType> *segments_head_, *segments_tail_;

    };

} // namespace alert

#endif
