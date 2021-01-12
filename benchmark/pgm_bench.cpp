//
// Created by Cshuang on 2020/10/15.
//
#include <thread>
#include <iostream>
#include <map>
#include <chrono>
#include "PGM-index/include/pgm/pgm_index.hpp"
#include "util.h"
using namespace std;

// First bulk load 200M key value pairs,
// then perform 10M point lookup in `zipf` distribution
template<typename KeyType, typename ValueType>
void ReadOnlyBenchmark(const string data_file, const Config &config) {
    // Load data
    vector<KeyType> origin_keys = util::load_data<KeyType>(data_file);
    auto keys = vector<KeyType>(origin_keys.begin(), origin_keys.begin() + config.init_num_keys);
    auto values = util::make_key_value<KeyType, ValueType>(keys);

    // Build
    auto build_begin = chrono::high_resolution_clock::now();
    pgm::PGMIndex<KeyType> pgm_;
    pgm_ = decltype(pgm_)(keys.begin(), keys.end());
    auto build_end = chrono::high_resolution_clock::now();

    // Point queries
    vector<KeyType> lookup_keys;
    util::generate_point_lookup<KeyType>(keys, lookup_keys, config.num_operations, config.lookup_distribution);

    auto lookup_begin = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < lookup_keys.size(); ++i) {
        auto approx_range = pgm_.search(lookup_keys[i]);
        auto lo = approx_range.lo;
        auto hi = approx_range.hi;
        auto it = std::lower_bound(keys.begin()+lo, keys.begin()+hi, lookup_keys[i]);
    }
    auto lookup_end = chrono::high_resolution_clock::now();

    // Range queries
    vector<RangeLookup<KeyType>> range_lookup = util::generate_range_lookups<KeyType>(keys, keys.size(), config.num_operations, config.max_range, config.lookup_distribution);

    auto range_lookup_begin = chrono::high_resolution_clock::now();
    for (const RangeLookup<KeyType>& lookup_iter : range_lookup) {
        auto approx_range = pgm_.search(lookup_iter.start);
        auto lo = approx_range.lo;
        auto hi = approx_range.hi;
        auto it = std::lower_bound(keys.begin()+lo, keys.begin()+hi, lookup_iter.start);
        std::vector<std::pair<KeyType, ValueType>> kvs;
        kvs.reserve(config.max_range+1);
        for (; it != keys.end() && *it < lookup_iter.end; ++it) {
            kvs.push_back(values[it-keys.begin()]);
        }
    }
    auto range_lookup_end = chrono::high_resolution_clock::now();

    uint64_t build_ns = chrono::duration_cast<chrono::nanoseconds>(build_end - build_begin).count();
    uint64_t lookup_ns = chrono::duration_cast<chrono::nanoseconds>(lookup_end - lookup_begin).count();
    uint64_t range_lookup_ns = chrono::duration_cast<chrono::nanoseconds>(range_lookup_end - range_lookup_begin).count();

    cout << "index:PGM"
         << " data_file:" << util::get_file_name(data_file)
         << " used_memory[MB]:" << (pgm_.size_in_bytes() / 1000.0) / 1000.0
         << " build_time[s]:" << (build_ns / 1000.0 / 1000.0) / 1000.0
         << " ns/lookup:" << lookup_ns / lookup_keys.size()
         << " ns/range:" << range_lookup_ns / range_lookup.size()
         << endl;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "usage: " << argv[0] << " <data_file> " << argv[1] << " <workload>" << endl;
        throw;
    }
    const string data_file = argv[1];
    const string workload_type = argv[2];

    util::set_cpu_affinity(0);

    Config config = util::get_config(workload_type);
    switch (config.workload_type) {
        case WorkloadType::READ_ONLY: {
            ReadOnlyBenchmark<uint64_t, uint64_t>(data_file, config);
            break;
        }
    }
    return 0;
}
