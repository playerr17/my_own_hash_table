/*
    Created by Askar Tsyganov on 11.01.2023.

    This hash table is based on the DySECT hash table from https://arxiv.org/abs/1705.00997
    I modified it to use a list instead of constant array and to use a single hash function instead of a few hash functions.
*/

#ifndef MY_OWN_HASH_TABLE_HASH_MAP_H
#define MY_OWN_HASH_TABLE_HASH_MAP_H

#include <iostream>

#include <array>
#include <exception>
#include <iterator>
#include <list>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

const size_t SUBTABLES_NUMBER = 1 << 8;
const size_t BUCKET_SIZE = 1 << 2;  // TODO: make it a parameter 2
const size_t HASH_NUMBER = 1;

template<class KeyType, class ValueType, class Hash = std::hash<KeyType> >
class HashMap {
public:
    class iterator;

    class const_iterator;

    explicit HashMap(Hash hasher = Hash()) {}

    HashMap(std::iterator<std::forward_iterator_tag, std::pair<KeyType, ValueType>> begin,
            std::iterator<std::forward_iterator_tag, std::pair<KeyType, ValueType>> end,
            Hash hasher = Hash()) {}

    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> list, Hash hasher = Hash()) {}

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    Hash hash_function() const {
        return hasher_;
    }

    void insert(std::pair<KeyType, ValueType> element) {
        size_t hash = hasher_(element.first);
        Subtable_& subtable = GetOptimalSubtable(hash);
        Bucket_& bucket = subtable.buckets_.get()[hash & ((1 << subtable.power_of_two_) - 1)];
        Element_ element_ = {element.first, element.second};
        if (!CheckElementInTable(element.first)) {
            InsertElementInBucket(bucket, element_, hash, subtable.power_of_two_);
            ++size_;
        }
        if (bucket.CheckFull(BUCKET_SIZE)) {
            ++subtable.full_buckets_;
            CheckFullSubtable(subtable);
        }
    }

    void erase(KeyType key) {
        size_t hash = hasher_(key);
        for (size_t i = 0; i < HASH_NUMBER; ++i) {
            auto& subtable = subtables_[GetHash(hash, i)];
            Bucket_& bucket = subtable.buckets_.get()[hash & ((1 << subtables_[GetHash(hash)].power_of_two_) - 1)];
            for (auto list_iterator = bucket.data_.begin(); list_iterator != bucket.data_.end(); ++list_iterator) {
                if (list_iterator->first == key) {
                    bucket.data_.erase(list_iterator);
                    --size_;
                    return;
                }
            }
        }
    }

    iterator find(KeyType key) {
    }

    const_iterator find(KeyType key) const {
    }

    ValueType &operator[](KeyType key) {
        try {
            auto& element = FindElement(key);
            return element.second;
        } catch (std::out_of_range& exception) {
            insert(std::make_pair(key, ValueType()));
            return FindElement(key).second;
        }
    }

    const ValueType &at(KeyType key) const {
        auto& element = FindElement(key);
        return element.second;
    }

    iterator begin() {
    }

    iterator end() {
    }

    const_iterator begin() const {
    }

    const_iterator end() const {
    }

    void clear() {
        for (size_t i = 0; i < SUBTABLES_NUMBER; ++i) {
            subtables_[i].buckets_.reset(new Bucket_[1]);
            subtables_[i].power_of_two_ = 0;
            subtables_[i].full_buckets_ = 0;
        }
        size_ = 0;
    }

private:
    using Element_ = std::pair<const KeyType, ValueType>;

    struct Bucket_ {
        std::list<Element_> data_;
        bool is_full_ = false;
        size_t size_left_ = 0;
        size_t size_right_ = 0;


        Bucket_() = default;

        bool CheckFull(size_t capacity) {
            if (!is_full_ && size_left_ >= capacity && size_right_ >= capacity) {  // TODO: make it OR
                is_full_ = true;
                return true;
            }
            return false;
        }
    };

    struct Subtable_ {
        std::unique_ptr<Bucket_[]> buckets_;
        size_t subtable_size_ = 1;
        size_t full_buckets_ = 0;
        size_t power_of_two_ = 0;

        Subtable_() {
            buckets_ = std::make_unique<Bucket_[]>(1);
        }
    };

    std::array<Subtable_, SUBTABLES_NUMBER> subtables_;
    Hash hasher_;
    size_t size_ = 0;
    double load_factor_ = 0.8;

    size_t GetHash(size_t hash) const {  // TODO: change to 64 bit hash
        return (((hash >> 32) & ((1 << 8) - 1))) ^ (((hash >> 16) & ((1 << 8) - 1))) ^
        (((hash >> 8) & ((1 << 8) - 1))) ^ (hash & ((1 << 8) - 1));
    }

    size_t GetHash(size_t hash, size_t i) const {
        return GetHash((hash >> 32) + (hash & ((1ll << 32) - 1)) * (i + 1));
    }

    Element_& FindElement(KeyType key) const {
        size_t hash = hasher_(key);
        for (size_t i = 0; i < HASH_NUMBER; ++i) {
            auto& subtable = subtables_[GetHash(hash, i)];
            auto& bucket = subtable.buckets_.get()[hash & ((1 << subtable.power_of_two_) - 1)];
            for (auto& element : bucket.data_) {
                if (element.first == key) {
                    return element;
                }
            }
        }
        throw std::out_of_range("Key not found");
    }

    void CheckFullSubtable(Subtable_& subtable) {
        if (subtable.full_buckets_ < subtable.subtable_size_ * load_factor_) {
            return;
        }
        std::unique_ptr<Bucket_[]> new_buckets = std::make_unique<Bucket_[]>(subtable.subtable_size_ * 2);
        for (size_t i = 0; i < subtable.subtable_size_; ++i) {
            for (auto& element : subtable.buckets_.get()[i].data_) {
                size_t hash = hasher_(element.first);
                Bucket_& bucket = new_buckets.get()[hash & ((1 << (subtable.power_of_two_ + 1)) - 1)];
                InsertElementInBucket(bucket, element, hash, subtable.power_of_two_ + 1);
            }
        }
        subtable.buckets_.reset(new_buckets.release());
        subtable.subtable_size_ *= 2;
        ++subtable.power_of_two_;
    }

    void InsertElementInBucket(Bucket_& bucket, Element_& element, size_t hash, size_t power_of_two_) {
        bucket.data_.push_front(element);
        if (((hash >> power_of_two_) & 1) == 0) {
            ++bucket.size_left_;
        } else {
            ++bucket.size_right_;
        }
    }

    bool CheckElementInTable(KeyType key) const {
        size_t hash = hasher_(key);
        for (size_t i = 0; i < HASH_NUMBER; ++i) {
            auto& subtable = subtables_[GetHash(hash, i)];
            auto& bucket = subtable.buckets_.get()[hash & ((1 << subtable.power_of_two_) - 1)];
            for (auto& element : bucket.data_) {
                if (element.first == key) {
                    return true;
                }
            }
        }
        return false;
    }

    Subtable_& GetOptimalSubtable(size_t hash) {
        size_t optimal_subtable = GetHash(hash, 0);
        size_t optimal_subtable_size = subtables_[GetHash(hash, 0)].subtable_size_;
        for (size_t i = 1; i < HASH_NUMBER; ++i) {
            if (subtables_[GetHash(hash, i)].subtable_size_ < optimal_subtable_size) {
                optimal_subtable = GetHash(hash, i);
                optimal_subtable_size = subtables_[GetHash(hash, i)].subtable_size_;
            }
        }
        return subtables_[optimal_subtable];
    }

    Element_& GetFirstElementInBucketAfter(Bucket_& bucket, auto iter) {

    }

public:
    class iterator {
    public:
        iterator() = default;

        explicit iterator(Element_* ptr) : ptr_(ptr) {}

        iterator& operator++() {

            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++ptr_;
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return ptr_ == other.ptr_;
        }

        bool operator!=(const iterator& other) const {
            return ptr_ != other.ptr_;
        }

        Element_& operator*() const {
            return *ptr_;
        }

        Element_* operator->() const {
            return ptr_;
        }

    private:
        Element_* ptr_;
        Subtable_* subtable_;
        size_t subtable_number_;
        size_t bucket_number_;
        typename std::list<std::pair<const KeyType, ValueType>>::iterator list_iterator_;

    };

    class const_iterator {
    public:
        const_iterator() = default;

        explicit const_iterator(const Element_* ptr) : ptr_(ptr) {}

        const_iterator& operator++() {
            ++ptr_;
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++ptr_;
            return tmp;
        }

        bool operator==(const const_iterator& other) const {
            return ptr_ == other.ptr_;
        }

        bool operator!=(const const_iterator& other) const {
            return ptr_ != other.ptr_;
        }

        const Element_& operator*() const {
            return *ptr_;
        }

        const Element_* operator->() const {
            return ptr_;
        }

    private:
        Element_* ptr_;
        Subtable_* subtable_;
        size_t subtable_number_;
        size_t bucket_number_;
        typename std::list<std::pair<const KeyType, ValueType>>::iterator list_iterator_;

    };

};

#endif //MY_OWN_HASH_TABLE_HASH_MAP_H
