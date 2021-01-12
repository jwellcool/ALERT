//
// Created by Cshuang on 2020/10/30.
//

#ifndef ARTS_UTIL_H
#define ARTS_UTIL_H

#include <algorithm>
#include <chrono>
#include <iostream>
#include <functional>
#include <fstream>
#include <thread>
#include <vector>
#include <cassert>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "zipf.h"
using std::vector;

#define ROW_WIDTH 1

enum DataType {
    UINT32 = 0,
    UINT64 = 1
};

enum WorkloadType {
    READ_ONLY = 0,
    READ_HEAVY = 1,
    SMALL_RANGE = 2,
    WRITE_HEAVY = 3,
    WRITE_ONLY = 4,
    READ_RANGE_WRITE = 5
};


class Config {
public:
    size_t init_num_keys = 200000000;
    int num_operations = 10000000;
    int max_range = 100;
    int batch_size = num_operations / 2;
    double insert_frac = 0.0;
    double range_frac = 0.0;
    std::string lookup_distribution = "zipf";
    std::string insert_distribution = "uniform";
    WorkloadType workload_type = WorkloadType::READ_ONLY;
};

template<typename KeyType, typename ValueType>
struct Row {
    KeyType key;
    ValueType data[ROW_WIDTH];
};

template<typename KeyType>
struct RangeLookup {
    KeyType start;
    KeyType end;
};


namespace util {

    static void fail(const std::string& message) {
        std::cerr << message << std::endl;
        exit(EXIT_FAILURE);
    }


    // Pins the current thread to core `core_id`.
    static void set_cpu_affinity(const uint32_t core_id) __attribute__((unused));

    static void set_cpu_affinity(const uint32_t core_id) {
#ifdef __linux__
        cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(core_id % std::thread::hardware_concurrency(), &mask);
  const int result = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
  if (result != 0)
    fail("failed to set CPU affinity");
#else
        (void) core_id;
        std::cout << "we only support thread pinning under Linux" << std::endl;
#endif
    }

    static uint64_t timing(std::function<void()> fn) {
        const auto start = std::chrono::high_resolution_clock::now();
        fn();
        const auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - start).count();
    }

    // Loads values from binary file into vector.
    template<typename T>
    static std::vector<T> load_data(const std::string &filename,
                                    bool print = false) {
        std::vector<T> data;
        const uint64_t ns = util::timing([&] {
            std::ifstream in(filename, std::ios::binary);
            if (!in.is_open()) {
                std::cerr << "unable to open " << filename << std::endl;
                exit(EXIT_FAILURE);
            }
            // Read size.
            uint64_t size;
            in.read(reinterpret_cast<char *>(&size), sizeof(uint64_t));

            data.resize(size);
            // Read values.
            in.read(reinterpret_cast<char *>(data.data()), size * sizeof(T));
            in.close();
        });
        const uint64_t ms = ns / 1e6;

        if (print) {
            std::cout << "read " << data.size() << " values from " << filename << " in "
                      << ms << " ms (" << static_cast<double>(data.size()) / 1000 / ms
                      << " M values/s)" << std::endl;
        }

        return data;
    }

    // Generates deterministic values for keys.
    template<typename KeyType, typename ValueType>
    static vector<std::pair<KeyType, ValueType>> make_key_value(const vector<KeyType> &keys) {
        vector<std::pair<KeyType, ValueType>> result;
        result.reserve(keys.size());

        for (ValueType i = 0; i < keys.size(); ++i) {
            result.push_back({keys[i], i});
        }
        return result;
    }

    // Generates deterministic values for keys.
    template<typename KeyType, typename ValueType>
    static vector<ValueType> make_values(const vector<KeyType>& keys) {
        vector<ValueType> result;
        result.reserve(keys.size());

        for (ValueType i = 0; i < keys.size(); ++i) {
            result.push_back(i);
        }
        return result;
    }

    static std::string get_file_name(const std::string &str) {
        std::stringstream ss(str);
        std::string tmp;
        std::vector<std::string> result;

        while (std::getline(ss, tmp, '/')) {
            result.push_back(tmp);
        }
        return result.back();
    }

    // Returns a duplicate-free copy.
    // Note that data has to be sorted.
    template<typename T>
    static std::vector<T> remove_duplicates(const std::vector<T>& data, size_t size) {
        std::vector<T> result = data;
        auto last = std::unique(result.begin(), result.begin()+size);
        result.erase(last, result.begin()+size);
        return result;
    }

    static Config get_config(const std::string &workload_type) {
        Config config;
        if (workload_type == "ro") { // read only
            config.workload_type = WorkloadType::READ_ONLY;
            return config;
        } else if (workload_type == "rh") { // read heavy
            config.workload_type = WorkloadType::READ_HEAVY;
            config.insert_frac = 0.05;
            return config;
        } else if (workload_type == "sr") { // small range
            config.workload_type = WorkloadType::SMALL_RANGE;
            config.insert_frac = 0.05;
            config.range_frac = 1.0;
            return config;
        } else if (workload_type == "wh") { // write heavy
            config.workload_type = WorkloadType::WRITE_HEAVY;
            config.insert_frac = 0.5;
            return config;
        }else if (workload_type == "wo") { // write only
            config.workload_type = WorkloadType::WRITE_ONLY;
            config.insert_frac = 1.0;
            return config;
        } else  if (workload_type == "rrw") { // read range write
            config.workload_type = WorkloadType::SMALL_RANGE;
            config.insert_frac = 0.4;
            config.range_frac = 0.5;
            return config;
        } else {
            std::cerr << "workload type " << workload_type << " not supported" << std::endl;
            exit(EXIT_FAILURE);
        }
    }


    // Based on: https://en.wikipedia.org/wiki/Xorshift
    class FastRandom {
    public:
        explicit FastRandom(uint64_t seed = 2305843008139952128ull) // The 8th perfect number found 1772 by Euler with <3
                : seed(seed) {}
        uint32_t RandUint32() {
            seed ^= (seed << 13);
            seed ^= (seed >> 15);
            return (uint32_t) (seed ^= (seed << 5));
        }
        int32_t RandInt32() { return (int32_t) RandUint32(); }
        uint32_t RandUint32(uint32_t inclusive_min, uint32_t inclusive_max) {
            return inclusive_min + RandUint32()%(inclusive_max - inclusive_min + 1);
        }
        int32_t RandInt32(int32_t inclusive_min, int32_t inclusive_max) {
            return inclusive_min + RandUint32()%(inclusive_max - inclusive_min + 1);
        }
        float RandFloat(float inclusive_min, float inclusive_max) {
            return inclusive_min + ScaleFactor()*(inclusive_max - inclusive_min);
        }
        // returns float between 0 and 1
        float ScaleFactor() {
            return static_cast<float>(RandUint32())
                   /std::numeric_limits<uint32_t>::max();
        }
        bool RandBool() { return RandUint32()%2==0; }

        uint64_t seed;

        static constexpr uint64_t Min() { return 0; }
        static constexpr uint64_t Max() { return std::numeric_limits<uint64_t>::max(); }
    };

    // Generates `num_lookups` lookups that satisfies `zipf` distribution.
    template<typename KeyType>
    void generate_point_lookup(vector<KeyType>& keys, vector<KeyType>& lookup_keys,
                         const size_t num_lookups, const std::string lookup_distribution) {


        if (lookup_distribution == "uniform") {
            util::FastRandom ranny(42);
            lookup_keys.resize(num_lookups);
            for (uint32_t i = 0; i < num_lookups; i++) {
                int pos = ranny.RandUint32(0, num_lookups);
                lookup_keys[i] = keys[pos];
            }
        } else if (lookup_distribution  == "zipf") {

            ScrambledZipfianGenerator zipf_gen(keys.size());

            lookup_keys.resize(num_lookups);

            for (int i = 0; i < num_lookups; i++) {
                int pos = zipf_gen.nextValue();
                lookup_keys[i] = keys[pos];
            }

        } else {
            std::cerr << "--lookup_distribution must be either 'uniform' or 'zipf'"
                      << std::endl;
            return ;
        }

    }

    template<typename KeyType>
    void generate_insert(vector<KeyType>& keys, vector<KeyType>& insert_keys,
                               const size_t num_inserts, const std::string insert_distribution) {

        if (insert_distribution == "uniform") {
            util::FastRandom ranny(42);
            insert_keys.resize(num_inserts);
            for (uint32_t i = 0; i < num_inserts; i++) {
                int pos = ranny.RandUint32(0, num_inserts);
                insert_keys[i] = keys[pos];
            }
        } else if (insert_distribution  == "zipf") {

            ScrambledZipfianGenerator zipf_gen(keys.size());

            insert_keys.resize(num_inserts);

            for (int i = 0; i < num_inserts; i++) {
                int pos = zipf_gen.nextValue();
                insert_keys[i] = keys[pos];
            }
        } else {
            std::cerr << "--lookup_distribution must be either 'uniform' or 'zipf'"
                      << std::endl;
            return ;
        }
    }

    template<typename KeyType>
    vector<RangeLookup<KeyType>> generate_range_lookups(const vector<KeyType>& keys, size_t size,
                                                                   const size_t num_range, const size_t max_range,  std::string lookup_distribution) {

        vector<RangeLookup<KeyType>> lookups;
        lookups.reserve(num_range);

        size_t num_generated = 0;

        // Get duplicate-free copy: we draw lookup keys from unique keys.
        vector<KeyType> unique_keys = remove_duplicates(keys, size);

        ScrambledZipfianGenerator zipf_gen(unique_keys.size());
        util::FastRandom ranny(42);

        while (num_generated < num_range) {

            // Draw lookup key from unique keys.
            uint64_t start_offset;
            if (lookup_distribution == "uniform") {
                start_offset = ranny.RandUint32(0, unique_keys.size() - 1);
            } else if (lookup_distribution  == "zipf") {
                start_offset = zipf_gen.nextValue();
            } else {
                std::cerr << "--lookup_distribution must be either 'uniform' or 'zipf'"
                          << std::endl;
                return {};
            }

            const uint64_t range_num = ranny.RandUint32(0, max_range);
            KeyType start_key = unique_keys[start_offset], end_key = start_key;

            // Perform binary search on original keys.
            auto it = lower_bound(keys.begin(), keys.begin()+size, start_key);
            uint64_t ele_num = 0;
            for (; it != keys.begin()+size && ele_num < range_num; ++it) {
                ++ele_num;
                end_key = *it;
            }

            lookups.push_back({start_key, end_key});
            ++num_generated;
        }
        return lookups;
    }

    template<typename KeyType>
    vector<RangeLookup<KeyType>> generate_range_lookups(const vector<KeyType>& keys, size_t size,
                                                        const size_t num_range, const size_t max_range,  std::string lookup_distribution, int seed) {

        vector<RangeLookup<KeyType>> lookups;
        lookups.reserve(num_range);

        size_t num_generated = 0;

        // Get duplicate-free copy: we draw lookup keys from unique keys.
        vector<KeyType> unique_keys = remove_duplicates(keys, size);

        ScrambledZipfianGenerator zipf_gen(unique_keys.size(), seed);
        util::FastRandom ranny(42);

        while (num_generated < num_range) {

            // Draw lookup key from unique keys.
            uint64_t start_offset;
            if (lookup_distribution == "uniform") {
                start_offset = ranny.RandUint32(0, unique_keys.size() - 1);
            } else if (lookup_distribution  == "zipf") {
                start_offset = zipf_gen.nextValue();
            } else {
                std::cerr << "--lookup_distribution must be either 'uniform' or 'zipf'"
                          << std::endl;
                return {};
            }

            const uint64_t range_num = ranny.RandUint32(0, max_range);
            KeyType start_key = unique_keys[start_offset], end_key = start_key;

            // Perform binary search on original keys.
            auto it = lower_bound(keys.begin(), keys.begin()+size, start_key);
            uint64_t ele_num = 0;
            for (; it != keys.begin()+size && ele_num < range_num; ++it) {
                ++ele_num;
                end_key = *it;
            }

            lookups.push_back({start_key, end_key});
            ++num_generated;
        }
        return lookups;
    }

    template <class T>
    T* get_search_keys(vector<T> &array, int num_keys, int num_searches) {
        std::mt19937_64 gen(42);
        std::uniform_int_distribution<int> dis(0, num_keys - 1);
        auto* keys = new T[num_searches];
        for (int i = 0; i < num_searches; i++) {
            int pos = dis(gen);
            keys[i] = array[pos];
        }
        return keys;
    }

    template <class T>
    T* get_search_keys(vector<T> &array, int num_keys, int num_searches, int seed) {
        std::mt19937_64 gen(seed);
        std::uniform_int_distribution<int> dis(0, num_keys - 1);
        auto* keys = new T[num_searches];
        for (int i = 0; i < num_searches; i++) {
            int pos = dis(gen);
            keys[i] = array[pos];
        }
        return keys;
    }

    template <class T>
    T* get_search_keys_zipf(vector<T> &array, int num_keys, int num_searches) {
        auto* keys = new T[num_searches];
        ScrambledZipfianGenerator zipf_gen(num_keys);
        for (int i = 0; i < num_searches; i++) {
            int pos = zipf_gen.nextValue();
            keys[i] = array[pos];
        }
        return keys;
    }

    template <class T>
    T* get_search_keys_zipf(vector<T> &array, int num_keys, int num_searches, int seed) {
        auto* keys = new T[num_searches];
        ScrambledZipfianGenerator zipf_gen(num_keys, seed);
        for (int i = 0; i < num_searches; i++) {
            int pos = zipf_gen.nextValue();
            keys[i] = array[pos];
        }
        return keys;
    }

    template<typename KeyType>
    void sample_keys(const vector<KeyType>& keys, size_t size, vector<KeyType>& sample, vector<KeyType>& remain,
                     const size_t num_sample) {
        int gap = size / num_sample;
        for (int i = 0; i < size; i++) {
            if (i % gap == 0) sample.push_back(keys[i]);
            else remain.push_back(keys[i]);
        }
    }
}
#endif //ARTS_UTIL_H
