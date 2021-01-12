//
// Created by Cshuang on 2020/12/7.
//
#include <iostream>
#include <map>
#include <chrono>
#include <thread>
#include <algorithm>
#include "alert-index.h"
#include "util.h"
using namespace std;

// First bulk load 200M key value pairs,
// then perform 10M point lookup in `zipf` distribution
template<typename KeyType, typename ValueType>
void DiffMaxErrorExperiment(const string data_file, const int max_error, const Config &config) {
    // Load data
    vector<KeyType> origin_keys = util::load_data<KeyType>(data_file);
    auto keys = vector<KeyType>(origin_keys.begin(), origin_keys.begin() + config.init_num_keys);
    auto values = util::make_values<KeyType, ValueType>(keys);

    // Build
    auto build_begin = chrono::high_resolution_clock::now();
    alert::Alert<KeyType, ValueType> index(max_error);
    index.BulkLoad(keys, values);
    auto build_end = chrono::high_resolution_clock::now();

    // Point queries
    vector<KeyType> lookup_keys;
    util::generate_point_lookup<KeyType>(keys, lookup_keys, config.num_operations, config.lookup_distribution);

    // total time
    auto lookup_begin = chrono::high_resolution_clock::now();
    ValueType v;
    for (size_t i = 0; i < lookup_keys.size(); ++i) {
        index.Find(lookup_keys[i], v);
    }
    auto lookup_end = chrono::high_resolution_clock::now();

    // segment search time
    void *vp;
    auto search_seg_begin = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < lookup_keys.size(); ++i) {
         vp = index.GetSplineSegment(lookup_keys[i]);
    }
    auto search_seg_end = chrono::high_resolution_clock::now();

    uint64_t build_ns = chrono::duration_cast<chrono::nanoseconds>(build_end - build_begin).count();
    uint64_t lookup_ns = chrono::duration_cast<chrono::nanoseconds>(lookup_end - lookup_begin).count();
    uint64_t search_seg_ns = chrono::duration_cast<chrono::nanoseconds>(search_seg_end - search_seg_begin).count();

    auto ns_per_lookup = lookup_ns / lookup_keys.size();
    auto ns_per_search_seg = search_seg_ns / lookup_keys.size();

    cout << "index:" + std::to_string(max_error)
         << " data_file:" << util::get_file_name(data_file)
         << " num-segs:" << index.num_seg()
         << " used_memory[MB]:" << (index.GetSizeInByte() / 1000.0) / 1000.0
         << " build_time[s]:" << (build_ns / 1000.0 / 1000.0) / 1000.0
         << " ns/lookup:" << ns_per_lookup
         << " ns/search-seg:" << ns_per_search_seg
         << " ns/in-seg-search:" << ns_per_lookup - ns_per_search_seg
         << endl;
}


int main(int argc, char** argv) {
    if (argc != 3) {
        cerr <<  "usage: " << argv[0] << " <data_file> " << argv[1] << " <max_error> " << argv[2] << endl;
        throw;
    }
    const string data_file = argv[1];
    const int max_error = stoi(argv[2]);

    util::set_cpu_affinity(0);

    Config config = util::get_config("ro");

    DiffMaxErrorExperiment<uint64_t, uint64_t>(data_file, max_error, config);
    return 0;
}

