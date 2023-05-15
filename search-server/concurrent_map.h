#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <tuple>

using namespace std::string_literals;
template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;

    };

    struct Access {
        Access(Bucket& val, const Key& key) : guard(val.mutex), ref_to_value(val.map[key]) {}
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : dsize(bucket_count), dictionaries(bucket_count) {};

    Access operator[](const Key& key) {
        uint64_t key64 = static_cast<uint64_t>(key);
        auto dnumber = key64 % dsize;
        return Access{ dictionaries[dnumber], key };
    };

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result; 
        for (auto& dictionary : dictionaries) {
            std::lock_guard guard(dictionary.mutex);
            result.insert(dictionary.map.begin(), dictionary.map.end());
        }
        return result;
    };
    void erase(const Key& key) {
        uint64_t key64 = static_cast<uint64_t>(key);
        auto dnumber = key64 % dsize;
        dictionaries[dnumber].map.erase(key);
    }
private:
    uint32_t dsize;
    std::vector<Bucket> dictionaries; 
};