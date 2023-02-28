#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>

namespace HashMapNamespace {
using HashType = uint64_t;


class Metadata {
public:
    using BucketHashType = uint8_t;

    using RiderMode = uint8_t;
    static constexpr RiderMode mode_empty = 0b0001;
    static constexpr RiderMode mode_hash = 0b0010;
    static constexpr RiderMode mode_full = 0b0100;
    static constexpr RiderMode mode_deleted = 0b1000;

    static constexpr BucketHashType get_empty();
    static BucketHashType get_hash(HashType hash);
    static constexpr BucketHashType get_deleted();

    template <RiderMode mode>
    class Rider {
    private:
        BucketHashType* metadata_ = nullptr;
        HashType hash_mask_ = 0;

        BucketHashType initial_hash_ = 0;
        HashType current_hash_ = 0;

    public:
        Rider();
        Rider(BucketHashType* metadata, HashType hash, HashType hash_mask);

        inline bool operator==(const Rider& other) const;
        inline bool operator!=(const Rider& other) const;

        inline size_t get_log_capacity() const;

        inline HashType get_current_hash() const;
        inline std::pair<HashType, RiderMode> next();
    };
};


template <class KeyValType>
class IteratorBase {
private:
    Metadata::Rider<Metadata::mode_full> rider_;
    KeyValType* data_;
    bool is_end_;

public:
    IteratorBase();
    IteratorBase(Metadata::BucketHashType* metadata, HashType index, HashType hash_mask, KeyValType* data, bool is_end);

    inline KeyValType& operator*();
    inline KeyValType* operator->();

    inline IteratorBase operator++();
    inline IteratorBase operator++(int);

    inline bool operator==(const IteratorBase& rhs) const;
    inline bool operator!=(const IteratorBase& rhs) const;

    inline size_t get_log_capacity() const;
};


template <class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
private:
    using KeyValType = std::pair<KeyType, ValueType>;

    Hash hasher_;

    size_t size_ = 0;
    size_t log_capacity_ = 0;
    Metadata::BucketHashType* metadata_ = nullptr;
    KeyValType* data_ = nullptr;
    size_t full_ = 0;

    inline void insert_generic(HashType index, const KeyValType& key_val);
    inline void erase_generic(HashType index);

    inline std::pair<bool, HashType> find_generic(const KeyType& key) const;
    inline std::pair<bool, HashType> find_allow_deleted_generic(const KeyType& key) const;

    template <class IteratorType>
    inline void copy_from_iterators_generic(IteratorType l, IteratorType r, size_t log_capacity);
    inline void resize(size_t new_log_capacity);

public:
    using iterator = IteratorBase<std::pair<const KeyType, ValueType>>;
    using const_iterator = IteratorBase<const std::pair<const KeyType, ValueType>>;

    HashMap(Hash hasher = Hash());

    HashMap(const HashMap& other);
    // HashMap(HashMap&& other);

    HashMap(iterator l, iterator r, Hash hasher = Hash());
    HashMap(iterator l, iterator r, size_t log_capacity, Hash hasher = Hash());
    HashMap(const_iterator l, const_iterator r, Hash hasher = Hash());
    HashMap(const_iterator l, const_iterator r, size_t log_capacity, Hash hasher = Hash());
    HashMap(std::initializer_list<KeyValType> l, Hash hasher = Hash());

    ~HashMap();

    inline const HashMap& operator=(const HashMap& other);
    // const HashMap& operator=(HashMap&& other);

    inline size_t size() const;
    inline bool empty() const;

    inline const Hash& hash_function() const;

    inline iterator begin();
    inline const_iterator begin() const;
    inline iterator end();
    inline const_iterator end() const;

    inline iterator find(const KeyType& key);
    inline const_iterator find(const KeyType& key) const;

    inline void insert(const KeyValType& key_val);
    inline void erase(const KeyType& key);

    inline ValueType& operator[](const KeyType& key);
    inline const ValueType& at(const KeyType& key) const;

    inline void clear();
    inline void full_clear();
};


constexpr Metadata::BucketHashType Metadata::get_empty() { return 0; }

Metadata::BucketHashType Metadata::get_hash(HashType hash) { return (hash & 0b1111111) + 1; }

constexpr Metadata::BucketHashType Metadata::get_deleted() { return -1; }

template <Metadata::RiderMode mode>
Metadata::Rider<mode>::Rider() = default;

template <Metadata::RiderMode mode>
Metadata::Rider<mode>::Rider(BucketHashType* metadata, HashType hash, HashType hash_mask)
    : metadata_(metadata), hash_mask_(hash_mask), initial_hash_(get_hash(hash)), current_hash_(hash - 1) {}

template <Metadata::RiderMode mode>
bool Metadata::Rider<mode>::operator==(const Rider& other) const {
    return std::tie(metadata_, current_hash_, hash_mask_) ==
           std::tie(other.metadata_, other.current_hash_, other.hash_mask_);
}

template <Metadata::RiderMode mode>
bool Metadata::Rider<mode>::operator!=(const Rider& other) const {
    return !(*this == other);
}

template <Metadata::RiderMode mode>
size_t Metadata::Rider<mode>::get_log_capacity() const {
    size_t log_capacity = 0;
    while ((1ull << log_capacity) <= hash_mask_) ++log_capacity;
    return log_capacity;
}


template <Metadata::RiderMode mode>
HashType Metadata::Rider<mode>::get_current_hash() const {
    return current_hash_;
}

template <Metadata::RiderMode mode>
std::pair<HashType, Metadata::RiderMode> Metadata::Rider<mode>::next() {
    do {
        current_hash_ = (current_hash_ + 1) & hash_mask_;
        switch (metadata_[current_hash_]) {
            case get_empty():
                if constexpr ((mode & Metadata::mode_empty) != 0) {
                    return {current_hash_, Metadata::mode_empty};
                }
                break;
            case get_deleted():
                if constexpr ((mode & Metadata::mode_deleted) != 0) {
                    return {current_hash_, Metadata::mode_deleted};
                }
                break;
            default:
                if constexpr ((mode & Metadata::mode_full) != 0) {
                    return {current_hash_, Metadata::mode_full};
                }
                if constexpr ((mode & Metadata::mode_hash) != 0) {
                    if (metadata_[current_hash_] == initial_hash_) {
                        return {current_hash_, Metadata::mode_hash};
                    }
                }
                break;
        }
    } while (true);
}


template <class KeyValType>
IteratorBase<KeyValType>::IteratorBase() = default;

template <class KeyValType>
IteratorBase<KeyValType>::IteratorBase(Metadata::BucketHashType* metadata, HashType index, HashType hash_mask,
                                       KeyValType* data, bool is_end)
    : rider_(metadata, index, hash_mask), data_(data), is_end_(is_end) {
    if (!is_end_) rider_.next();
}

template <class KeyValType>
KeyValType& IteratorBase<KeyValType>::operator*() {
    return data_[rider_.get_current_hash()];
}

template <class KeyValType>
KeyValType* IteratorBase<KeyValType>::operator->() {
    return &data_[rider_.get_current_hash()];
}

template <class KeyValType>
IteratorBase<KeyValType> IteratorBase<KeyValType>::operator++() {
    HashType last_index = rider_.get_current_hash();
    rider_.next();
    if (rider_.get_current_hash() <= last_index) {
        is_end_ = true;
    }
    return *this;
}

template <class KeyValType>
IteratorBase<KeyValType> IteratorBase<KeyValType>::operator++(int) {
    IteratorBase<KeyValType> res = *this;
    ++(*this);
    return res;
}

template <class KeyValType>
bool IteratorBase<KeyValType>::operator==(const IteratorBase& rhs) const {
    if (is_end_ || rhs.is_end_) {
        return is_end_ && rhs.is_end_;
    } else {
        return std::tie(rider_, data_) == std::tie(rhs.rider_, rhs.data_);
    }
}

template <class KeyValType>
bool IteratorBase<KeyValType>::operator!=(const IteratorBase& rhs) const {
    return !(*this == rhs);
}

template <class KeyValType>
size_t IteratorBase<KeyValType>::get_log_capacity() const {
    return rider_.get_log_capacity();
}


template <class KeyType, class ValueType, class Hash>
void HashMap<KeyType, ValueType, Hash>::insert_generic(HashType index, const KeyValType& key_val) {
    ++size_;
    ++full_;
    metadata_[index] = Metadata::get_hash(hasher_(key_val.first));
    data_[index] = key_val;

    if (full_ > ((1ull << log_capacity_) >> 1ull)) {
        resize(log_capacity_ + 1);
    }
}

template <class KeyType, class ValueType, class Hash>
void HashMap<KeyType, ValueType, Hash>::erase_generic(HashType index) {
    --size_;
    metadata_[index] = Metadata::get_deleted();
    // delete data_[index];

    if (size_ < ((1ull << log_capacity_) >> 3ull)) {
        resize(log_capacity_ - 1);
    }
}

template <class KeyType, class ValueType, class Hash>
std::pair<bool, HashType> HashMap<KeyType, ValueType, Hash>::find_generic(const KeyType& key) const {
    HashType hash = hasher_(key);
    Metadata::Rider<Metadata::mode_empty | Metadata::mode_hash> rider(metadata_, hash, (1 << log_capacity_) - 1);
    do {
        std::pair<HashType, Metadata::RiderMode> p = rider.next();
        switch (p.second) {
            case Metadata::mode_empty:
                return {false, p.first};
            case Metadata::mode_hash:
                if (data_[p.first].first == key) {
                    return {true, p.first};
                }
                break;
        }
    } while (true);
}

template <class KeyType, class ValueType, class Hash>
std::pair<bool, HashType> HashMap<KeyType, ValueType, Hash>::find_allow_deleted_generic(const KeyType& key) const {
    HashType hash = hasher_(key);
    Metadata::Rider<Metadata::mode_empty | Metadata::mode_hash | Metadata::mode_deleted> rider(metadata_, hash, (1 << log_capacity_) - 1);
    bool not_found_empty_position = true;
    HashType empty_position;
    do {
        std::pair<HashType, Metadata::RiderMode> p = rider.next();
        switch (p.second) {
            case Metadata::mode_empty:
                if (not_found_empty_position) {
                    empty_position = p.first;
                }
                return {false, empty_position};
            case Metadata::mode_deleted:
                if (not_found_empty_position) {
                    empty_position = p.first;
                    not_found_empty_position = false;
                }
                break;
            case Metadata::mode_hash:
                if (data_[p.first].first == key) {
                    return {true, p.first};
                }
                break;
        }
    } while (true);
}

template <class KeyType, class ValueType, class Hash>
template <class IteratorType>
void HashMap<KeyType, ValueType, Hash>::copy_from_iterators_generic(IteratorType l, IteratorType r,
                                                                    size_t log_capacity) {
    full_clear();
    log_capacity_ = log_capacity;
    metadata_ = new Metadata::BucketHashType[1 << log_capacity_];
    memset(metadata_, Metadata::get_empty(), 1 << log_capacity_);
    data_ = new KeyValType[1 << log_capacity_];

    for (; l != r; ++l) {
        insert(*l);
    }
}

template <class KeyType, class ValueType, class Hash>
void HashMap<KeyType, ValueType, Hash>::resize(size_t new_log_capacity) {
    HashMap new_hashmap(begin(), end(), new_log_capacity, hasher_);
    std::swap(size_, new_hashmap.size_);
    std::swap(log_capacity_, new_hashmap.log_capacity_);
    std::swap(metadata_, new_hashmap.metadata_);
    std::swap(data_, new_hashmap.data_);
}

template <class KeyType, class ValueType, class Hash>
HashMap<KeyType, ValueType, Hash>::HashMap(Hash hasher) : hasher_(hasher) {
    log_capacity_ = 1;
    metadata_ = new Metadata::BucketHashType[1 << log_capacity_];
    memset(metadata_, Metadata::get_empty(), 1 << log_capacity_);
    data_ = new KeyValType[1 << log_capacity_];
}


template <class KeyType, class ValueType, class Hash>
HashMap<KeyType, ValueType, Hash>::HashMap(const HashMap& other)
    : hasher_(other.hasher_), size_(other.size_), log_capacity_(other.log_capacity_) {
    metadata_ = new Metadata::BucketHashType[1 << log_capacity_];
    data_ = new KeyValType[1 << log_capacity_];
    for (size_t i = 0; i < (1ull << log_capacity_); ++i) {
        metadata_[i] = other.metadata_[i];
        data_[i] = other.data_[i];
    }
}

// template <class KeyType, class ValueType, class Hash>
// HashMap<KeyType, ValueType, Hash>::HashMap(HashMap&& other)
//     : hasher_(other.hasher_),
//       size_(other.size_),
//       log_capacity_(other.log_capacity_),
//       metadata_(other.metadata_),
//       data_(other.data_) {}


template <class KeyType, class ValueType, class Hash>
HashMap<KeyType, ValueType, Hash>::HashMap(iterator l, iterator r, Hash hasher)
    : HashMap(l, r, l.get_log_capacity(), hasher) {}

template <class KeyType, class ValueType, class Hash>
HashMap<KeyType, ValueType, Hash>::HashMap(iterator l, iterator r, size_t log_capacity, Hash hasher)
    : hasher_(hasher), log_capacity_(log_capacity) {
    copy_from_iterators_generic(l, r, log_capacity);
}

template <class KeyType, class ValueType, class Hash>
HashMap<KeyType, ValueType, Hash>::HashMap(const_iterator l, const_iterator r, Hash hasher)
    : HashMap(l, r, l.get_log_capacity(), hasher) {}

template <class KeyType, class ValueType, class Hash>
HashMap<KeyType, ValueType, Hash>::HashMap(const_iterator l, const_iterator r, size_t log_capacity, Hash hasher)
    : hasher_(hasher), log_capacity_(log_capacity) {
    copy_from_iterators_generic(l, r, log_capacity);
}

template <class KeyType, class ValueType, class Hash>
HashMap<KeyType, ValueType, Hash>::HashMap(std::initializer_list<KeyValType> l, Hash hasher) : hasher_(hasher) {
    size_t log_capacity_ = 0;
    while ((1ull << log_capacity_) < l.size()) ++log_capacity_;
    ++log_capacity_;
    copy_from_iterators_generic(l.begin(), l.end(), log_capacity_);
}

template <class KeyType, class ValueType, class Hash>
HashMap<KeyType, ValueType, Hash>::~HashMap() {
    delete[] metadata_;
    delete[] data_;
}


template <class KeyType, class ValueType, class Hash>
const HashMap<KeyType, ValueType, Hash>& HashMap<KeyType, ValueType, Hash>::operator=(const HashMap& other) {
    if constexpr (std::is_copy_constructible<Hash>::value) {
        if (metadata_ == other.metadata_ && data_ == other.data_) {
            return *this;
        }

        full_clear();

        hasher_ = other.hasher_;
        size_ = other.size_;
        full_ = other.full_;
        log_capacity_ = other.log_capacity_;

        metadata_ = new Metadata::BucketHashType[1 << log_capacity_];
        data_ = new KeyValType[1 << log_capacity_];
        for (size_t i = 0; i < (1ull << log_capacity_); ++i) {
            metadata_[i] = other.metadata_[i];
            data_[i] = other.data_[i];
        }
    } else {
        assert(false);
    }
    return *this;
}

// template <class KeyType, class ValueType, class Hash>
// const HashMap<KeyType, ValueType, Hash>& HashMap<KeyType, ValueType, Hash>::operator=(HashMap&& other) {
//     clear();

//     hasher_ = other.hasher_;
//     size_ = other.size_;
//     log_capacity_ = other.log_capacity_;
//     metadata_ = other.metadata_;
//     data_ = other.data_;

//     return *this;
// }

template <class KeyType, class ValueType, class Hash>
size_t HashMap<KeyType, ValueType, Hash>::size() const {
    return size_;
}

template <class KeyType, class ValueType, class Hash>
bool HashMap<KeyType, ValueType, Hash>::empty() const {
    return !size_;
}


template <class KeyType, class ValueType, class Hash>
const Hash& HashMap<KeyType, ValueType, Hash>::hash_function() const {
    return hasher_;
}


template <class KeyType, class ValueType, class Hash>
typename HashMap<KeyType, ValueType, Hash>::iterator HashMap<KeyType, ValueType, Hash>::begin() {
    if (empty()) {
        return end();
    }
    return iterator(metadata_, 0, (1 << log_capacity_) - 1,
                    reinterpret_cast<std::pair<const KeyType, ValueType>*>(data_), false);
}

template <class KeyType, class ValueType, class Hash>
typename HashMap<KeyType, ValueType, Hash>::const_iterator HashMap<KeyType, ValueType, Hash>::begin() const {
    if (empty()) {
        return end();
    }
    return const_iterator(metadata_, 0, (1 << log_capacity_) - 1,
                          reinterpret_cast<std::pair<const KeyType, ValueType>*>(data_), false);
}

template <class KeyType, class ValueType, class Hash>
typename HashMap<KeyType, ValueType, Hash>::iterator HashMap<KeyType, ValueType, Hash>::end() {
    return iterator(metadata_, 0, (1 << log_capacity_) - 1,
                    reinterpret_cast<std::pair<const KeyType, ValueType>*>(data_), true);
}

template <class KeyType, class ValueType, class Hash>
typename HashMap<KeyType, ValueType, Hash>::const_iterator HashMap<KeyType, ValueType, Hash>::end() const {
    return const_iterator(metadata_, 0, (1 << log_capacity_) - 1,
                          reinterpret_cast<std::pair<const KeyType, ValueType>*>(data_), true);
}

template <class KeyType, class ValueType, class Hash>
typename HashMap<KeyType, ValueType, Hash>::iterator HashMap<KeyType, ValueType, Hash>::find(const KeyType& key) {
    std::pair<bool, HashType> p = find_generic(key);
    if (p.first) {
        return iterator(metadata_, p.second, (1 << log_capacity_) - 1,
                        reinterpret_cast<std::pair<const KeyType, ValueType>*>(data_), false);
    } else {
        return end();
    }
}

template <class KeyType, class ValueType, class Hash>
typename HashMap<KeyType, ValueType, Hash>::const_iterator HashMap<KeyType, ValueType, Hash>::find(
    const KeyType& key) const {
    std::pair<bool, HashType> p = find_generic(key);
    if (p.first) {
        return const_iterator(metadata_, p.second, (1 << log_capacity_) - 1,
                              reinterpret_cast<std::pair<const KeyType, ValueType>*>(data_), false);
    } else {
        return end();
    }
}


template <class KeyType, class ValueType, class Hash>
void HashMap<KeyType, ValueType, Hash>::insert(const KeyValType& key_val) {
    std::pair<bool, HashType> p = find_allow_deleted_generic(key_val.first);
    if (!p.first) {
        insert_generic(p.second, key_val);
    }
}

template <class KeyType, class ValueType, class Hash>
void HashMap<KeyType, ValueType, Hash>::erase(const KeyType& key) {
    std::pair<bool, HashType> p = find_generic(key);
    if (p.first) {
        erase_generic(p.second);
    }
}

template <class KeyType, class ValueType, class Hash>
ValueType& HashMap<KeyType, ValueType, Hash>::operator[](const KeyType& key) {
    std::pair<bool, HashType> p = find_allow_deleted_generic(key);
    if (!p.first) {
        insert_generic(p.second, {key, ValueType()});
    }
    p = find_allow_deleted_generic(key);
    return data_[p.second].second;
}

template <class KeyType, class ValueType, class Hash>
const ValueType& HashMap<KeyType, ValueType, Hash>::at(const KeyType& key) const {
    std::pair<bool, HashType> p = find_generic(key);
    if (p.first) {
        return data_[p.second].second;
    }
    throw std::out_of_range("No such key!");
}

template <class KeyType, class ValueType, class Hash>
void HashMap<KeyType, ValueType, Hash>::clear() {
    delete[] metadata_;
    delete[] data_;

    size_ = 0;
    full_ = 0;
    log_capacity_ = 1;
    metadata_ = new Metadata::BucketHashType[1 << log_capacity_];
    memset(metadata_, Metadata::get_empty(), 1 << log_capacity_);
    data_ = new KeyValType[1 << log_capacity_];
}

template <class KeyType, class ValueType, class Hash>
void HashMap<KeyType, ValueType, Hash>::full_clear() {
    delete[] metadata_;
    delete[] data_;

    size_ = 0;
    full_ = 0;
    log_capacity_ = 0;
    metadata_ = nullptr;
    data_ = nullptr;
}
}  // namespace HashMapNamespace

using HashMapNamespace::HashMap;
