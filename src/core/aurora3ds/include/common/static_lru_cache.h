#pragma once

#include <map>

namespace Common {

template <typename Key, typename Value, std::size_t Capacity>
class StaticLRUCache {
public:
    std::pair<bool, Value&> request(const Key& key) {
        auto it = storage.find(key);
        if (it != storage.end()) {
            return {true, it->second};
        }
        if (storage.size() >= Capacity && !storage.empty()) {
            storage.erase(storage.begin());
        }
        auto [ins, _] = storage.emplace(key, Value{});
        return {false, ins->second};
    }

    bool contains(const Key& key) const {
        return storage.find(key) != storage.end();
    }

    void clear() {
        storage.clear();
    }

private:
    std::map<Key, Value> storage;
};

} // namespace Common
