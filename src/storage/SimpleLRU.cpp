#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

std::reference_wrapper<SimpleLRU::lru_node> SimpleLRU::GetNode(std::string key, bool& success) {
    if (_lru_index.count(key) == 0) {
        success = false;
        lru_node node;
        return std::reference_wrapper<lru_node>(node);
    }
    success = true;
    return _lru_index.find(key)->second;
}

bool SimpleLRU::RemoveOldNodes(const size_t size) {
    if (size > _max_size) {
        return false;
    }
    while (_lru_tail && _cur_size < size){
        if (!SimpleLRU::Delete(_lru_head->key)) {
            return false;
        }
    }
    return true;
}

void SimpleLRU::FillNode(
    std::shared_ptr<lru_node> node,
    const std::string &key,
    const std::string &value,
    std::shared_ptr<lru_node> prev,
    std::shared_ptr<lru_node> next
) {
    node->key = key;
    node->value = value;
    node->prev = prev;
    node->next = next;
    if (prev) {
        prev->next = node;
    }
    if (next) {
        next->prev = node;
    }
    _lru_index.insert(std::pair<std::reference_wrapper<std::string>, std::reference_wrapper<lru_node> >(node->key, *node));
}

bool SimpleLRU::PutAnyway(const std::string &key, const std::string &value) {
    if (!SimpleLRU::RemoveOldNodes(key.size() + value.size())) {
        return false;
    }
    if (_lru_tail) {
        _lru_tail->next = std::make_shared<lru_node>(lru_node());
        SimpleLRU::FillNode(_lru_tail->next, key, value, _lru_tail, nullptr);
        _lru_tail = _lru_tail->next;
    } else {
        _lru_tail = _lru_head = std::make_shared<lru_node>(lru_node());
        SimpleLRU::FillNode(_lru_tail, key, value, nullptr, nullptr);
    }
     _cur_size -= key.size() + value.size();
    return true;

}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    bool success = false;
    std::reference_wrapper<lru_node> node = GetNode(key, success);
    if (success) {
        if (!SimpleLRU::Set(key, value)) {
            return false;
        }
    } else {
        if (!SimpleLRU::PutAnyway(key, value)) {
            return false;
        }
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    bool success = false;
    std::reference_wrapper<lru_node> node = GetNode(key, success);
    if (success) {
        return false;
    }
    if (!SimpleLRU::PutAnyway(key, value)) {
        return false;
    }
    return true;
}

void SimpleLRU::RearrangeToTail(std::reference_wrapper<lru_node> node) {
    std::shared_ptr<lru_node> next = node.get().next;
    if (next) {
        std::shared_ptr<lru_node> temp_lru_tail = _lru_tail, prev = node.get().prev;
        _lru_tail->next = next->prev;
        _lru_tail = _lru_tail->next;
        if (prev) {
            prev->next = next;
        } else {
            _lru_head = next;
        }
        next->prev = prev;
        _lru_tail->prev = temp_lru_tail;
        _lru_tail->next = nullptr;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    bool success = false;
    std::reference_wrapper<lru_node> node = GetNode(key, success);
    if (!success) {
        return false;
    }
    if (!SimpleLRU::RemoveOldNodes(value.size() - node.get().value.size())) {
        return false;
    }
    _cur_size += value.size() - node.get().value.size();
    node.get().value = value;

    SimpleLRU::RearrangeToTail(node);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    bool success = false;
    std::reference_wrapper<lru_node> node = GetNode(key, success);
    if (!success) {
        return false;
    }

    _cur_size += node.get().key.size() + node.get().value.size();
    std::shared_ptr<lru_node> prev = node.get().prev, next = node.get().next;
    auto index_node = _lru_index.find(node.get().key);
    _lru_index.erase(index_node);
    if (prev) {
        prev->next = next;
    } else {
        _lru_head = next;
    }
    if (next) {
        next->prev = prev;
    } else {
        _lru_tail = prev;
    }

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    bool success = false;
    std::reference_wrapper<lru_node> node = GetNode(key, success);
    if (!success) {
        return false;
    }
    value = node.get().value;

    SimpleLRU::RearrangeToTail(node);

    return true;
}

} // namespace Backend
} // namespace Afina
