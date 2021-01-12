//
// Created by Cshuang on 2020/10/27.
//

#ifndef ARTS_BUCKET_H
#define ARTS_BUCKET_H

#include <vector>
#include <forward_list>
#include <map>
#include <algorithm>
#include "stx/btree_multimap.h"

namespace alert {

    namespace {
        const float alpha = 0.02;

        template<typename KeyType, typename ValueType>
        class MFList {
            struct ListNode {
                KeyType key;
                ValueType value;
                ListNode *next = nullptr;
            };
            typedef ListNode *data_iterator;
        public:

            ~MFList() {
                ListNode *cur = dummy_.next, *next = nullptr;
                while (cur) {
                    next = cur->next;
                    delete cur;
                    cur = next;
                }
            }

            class iterator {
            public:
                iterator(data_iterator p) : _pcur(p) {}

                bool operator!=(const iterator &src) {
                    return _pcur != src._pcur;
                }

                void operator++() {
                    _pcur = _pcur->next;
                }

                ListNode &operator*() {
                    return *_pcur;
                }

                data_iterator pointer() {
                    return _pcur;
                }

                const ListNode &operator*() const {
                    return *_pcur;
                }

            private:
                data_iterator _pcur;
            };

            iterator before_begin() {
                return iterator(&dummy_);
            }

            iterator begin() {
                return iterator(dummy_.next);
            }

            iterator end() {
                return iterator(tail_->next);
            }

            inline void Insert(KeyType key, ValueType value) {
                window_sz_ += 1;
                tail_->next = new ListNode{key, value, nullptr};
                tail_ = tail_->next;
            }

            inline void ReuseInsert(KeyType key, ValueType value) {
                window_sz_ += 1;
                if (tail_->next) {
                    tail_->next->key = key;
                    tail_->next->value = value;
                } else {
                    tail_->next = new ListNode{key, value, nullptr};
                }
                tail_ = tail_->next;
            }

            inline bool Find(KeyType key, ValueType &value) {
                int dis = 0;
                for (ListNode *cur = dummy_.next, *pre = &dummy_; cur != tail_->next; pre = cur, cur = cur->next) {
                    if (cur->key == key) {
                        value = cur->value;
                        window_sz_ = alpha * window_sz_ + (1 - alpha) * dis;
                        if (dis > window_sz_) {
                            // return after do this, so has no problem.
                            MoveFrontAfter(pre);
                        }
//                        std::cout << dis << " -- " <<  window_sz_ << std::endl;
                        return true;
                    }
                    ++dis;
                }
                return false;
            }

            inline void Clear() {
                window_sz_ = 0;
                tail_ = &dummy_;
            }

            inline void EraseAfter(iterator &pre_it) {
                // erase
                ListNode *target = (*pre_it).next;
                if (target == tail_) tail_ = pre_it.pointer();
                else {
                    (*pre_it).next = target->next;
                    target->next = tail_->next;
                    tail_->next = target;
                }
            }

            inline size_t window_size() {
                return window_sz_;
            }

            inline bool Empty() {
                return tail_ == &dummy_;
            }

        private:

            inline void MoveFrontAfter(ListNode *pre) {
                // erase
                ListNode *target = pre->next;
                pre->next = target->next;
                // move to front
                ListNode *next_node = dummy_.next;
                dummy_.next = target;
                target->next = next_node;
            }

        private:
            ListNode dummy_;
            ListNode *tail_ = &dummy_;
            size_t window_sz_ = 0;
        };

        template<typename KeyType, typename ValueType>
        class OverflowBuffer {
            typedef std::pair<KeyType, ValueType> Entry;
        public:

            inline void Insert(KeyType key, ValueType value) {
                unordered_buffer_.Insert(key, value);
            }

            inline void ReuseInsert(KeyType key, ValueType value) {
                unordered_buffer_.ReuseInsert(key, value);
            }

            inline bool Find(KeyType key, ValueType &value) {
                if (!ordered_buffer_.empty()) {
                    auto it = ordered_buffer_.find(key);
                    if (it != ordered_buffer_.end()) {
                        value = it->second;
                        return true;
                    }
                }
                return unordered_buffer_.Find(key, value);
            }

            inline void Range(KeyType start_key, KeyType end_key, std::vector<Entry> &kvs, uint32_t &sorted_keys_num_) {
                auto pre_it = unordered_buffer_.before_begin();
                KeyType key;
                ValueType value;
                const auto end = unordered_buffer_.end(); // unordered_buffer_.end() maybe change when erase element in list, so we need to remember it.
                int cur_pos = kvs.size();
                for (auto it = unordered_buffer_.begin(); it != end;) {
                    key = (*it).key;
                    if (start_key <= key && key < end_key) {
                        value = (*it).value;
                        kvs.emplace_back(key, value);
//                        ordered_buffer_.insert(key, value);
                        ++it;
//                        unordered_buffer_.EraseAfter(pre_it);
//                        sorted_keys_num_ += 1;
                    } else {
                        pre_it = it;
                        ++it;
                    }
                }
              std::sort(kvs.begin()+cur_pos, kvs.end());

//                auto it = ordered_buffer_.lower_bound(start_key);
//                for (; it != ordered_buffer_.end() && it->first < end_key; ++it) {
//                    kvs.emplace_back(it->first, it->second);
//                }
            }

            inline void ToSortedData(std::vector<KeyType> &keys, std::vector<ValueType> &values) {
                for (auto it = unordered_buffer_.begin(); it != unordered_buffer_.end(); ++it) {
                    ordered_buffer_.insert((*it).key, (*it).value);
                }

                for (auto it = ordered_buffer_.begin(); it != ordered_buffer_.end(); ++it) {
                    keys.push_back(it->first);
                    values.push_back(it->second);
                }
            }

            inline bool Empty() {
                return unordered_buffer_.Empty() && ordered_buffer_.empty();
            }

            inline void Clear() {
                unordered_buffer_.Clear();
                ordered_buffer_.clear();
            }

            inline MFList<KeyType, ValueType>& unordered_buffer() {
                return unordered_buffer_;
            }

        private:
            MFList<KeyType, ValueType> unordered_buffer_;
            stx::btree_multimap<KeyType,
                    ValueType,
                    std::less<KeyType>,
                    stx::btree_default_map_traits<KeyType, ValueType>> ordered_buffer_;
        };

    }
}

#endif //ARTS_BUCKET_H
