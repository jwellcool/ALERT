//
// Created by Cshuang on 2020/10/15.
//
#include <thread>
#include <iostream>
#include <fstream>
#include <map>
#include <chrono>
#include "art_primary_lb.h"
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
    ARTPrimaryLB t;
    t.Build(values);
    auto build_end = chrono::high_resolution_clock::now();

    // Point queries
    vector<KeyType> lookup_keys;
    util::generate_point_lookup<KeyType>(keys, lookup_keys, config.num_operations, config.lookup_distribution);

    auto lookup_begin = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < lookup_keys.size(); ++i) {
        auto v = t.LowerBound(lookup_keys[i]);
    }
    auto lookup_end = chrono::high_resolution_clock::now();

    // Range queries
    vector<RangeLookup<KeyType>> range_lookup = util::generate_range_lookups<KeyType>(keys, keys.size(), config.num_operations, config.max_range, config.lookup_distribution);

    auto range_lookup_begin = chrono::high_resolution_clock::now();
    for (const RangeLookup<KeyType>& lookup_iter : range_lookup) {
        std::vector<std::pair<KeyType, uint64_t>> kvs;
        kvs.reserve(config.max_range+1);
        t.Range(lookup_iter.start, lookup_iter.end, kvs);
    }
    auto range_lookup_end = chrono::high_resolution_clock::now();

    uint64_t build_ns = chrono::duration_cast<chrono::nanoseconds>(build_end - build_begin).count();
    uint64_t lookup_ns = chrono::duration_cast<chrono::nanoseconds>(lookup_end - lookup_begin).count();
    uint64_t range_lookup_ns = chrono::duration_cast<chrono::nanoseconds>(range_lookup_end - range_lookup_begin).count();


    size_t data_size = t.size() - keys.size() * (sizeof(KeyType) + sizeof(ValueType));
    cout << "index:ART"
         << " data_file:" << util::get_file_name(data_file)
         << " used_memory[MB]:" << (data_size / 1000.0) / 1000.0
         << " build_time[s]:" << (build_ns / 1000.0 / 1000.0) / 1000.0
         << " ns/lookup:" << lookup_ns / lookup_keys.size()
         << " ns/range:" << range_lookup_ns / range_lookup.size()
         << endl;
}


template<typename KeyType, typename ValueType>
void ReadWriteBenchmark( const string data_file, const Config &config) {
    // Load data
    vector<KeyType> keys = util::load_data<KeyType>(data_file);

    auto init_keys = vector<KeyType>(keys.begin(), keys.begin() + config.init_num_keys);
    auto init_values = util::make_key_value<KeyType, ValueType>(init_keys);

    // Create and bulk load
    ARTPrimaryLB t;
    t.Build(init_values);

    // Run workload
    int total_num_keys = config.init_num_keys;
    long long cumulative_inserts = 0;
    long long cumulative_lookups = 0;
    long long cumulative_ranges = 0;
    int batch_size = config.batch_size;
    int num_inserts_per_batch = static_cast<int>(batch_size * config.insert_frac);
    int num_lookups_per_batch = batch_size - num_inserts_per_batch;
    int num_range_per_batch =  static_cast<int>(num_lookups_per_batch * config.range_frac);
    num_lookups_per_batch -= num_range_per_batch;
    double cumulative_insert_time = 0;
    double cumulative_lookup_time = 0;
    double cumulative_range_time = 0;

    int batch_no = 0;
    int total_batch_no = config.num_operations / config.batch_size;
    while (batch_no < total_batch_no) {
        batch_no++;
        // Do inserts
        // generate insert keys
        vector<KeyType> insert_keys;
        util::generate_insert<KeyType>(keys, insert_keys, num_inserts_per_batch, config.insert_distribution);

        auto inserts_start_time = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < num_inserts_per_batch; i++) {
            // Perform operation
            t.Insert(insert_keys[i], i);
        }
        auto inserts_end_time = std::chrono::high_resolution_clock::now();
        double batch_insert_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(inserts_end_time -
                                                                     inserts_start_time)
                        .count();
        cumulative_insert_time += batch_insert_time;
        cumulative_inserts += num_inserts_per_batch;

        // Do lookups
        KeyType* lookup_keys = nullptr;
        if (config.lookup_distribution == "uniform") {
            lookup_keys = util::get_search_keys(keys, total_num_keys, num_lookups_per_batch);
        } else if (config.lookup_distribution  == "zipf") {
            lookup_keys = util::get_search_keys_zipf(keys, total_num_keys, num_lookups_per_batch);
        } else {
            std::cerr << "--lookup_distribution must be either 'uniform' or 'zipf'"
                      << std::endl;
            return ;
        }
        auto lookups_start_time = std::chrono::high_resolution_clock::now();
        ValueType v;
        for (int j = 0; j < num_lookups_per_batch; j++) {
            // Perform operation
            auto v = t.LowerBound(lookup_keys[j]);
        }
        auto lookups_end_time = std::chrono::high_resolution_clock::now();
        double batch_lookup_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(lookups_end_time -
                                                                     lookups_start_time)
                        .count();
        cumulative_lookup_time += batch_lookup_time;
        cumulative_lookups += num_lookups_per_batch;
        delete[] lookup_keys;

        // Do range
        vector<RangeLookup<KeyType>> range_lookup = util::generate_range_lookups<KeyType>(keys, total_num_keys, num_range_per_batch, config.max_range, config.lookup_distribution);

        auto range_start_time = std::chrono::high_resolution_clock::now();
        for (int j = 0; j < num_range_per_batch; j++) {
            // Perform operation
            std::vector<std::pair<KeyType, uint64_t>> kvs;
            kvs.reserve(config.max_range+1);
            t.Range(range_lookup[j].start, range_lookup[j].end, kvs);
        }
        auto range_end_time = std::chrono::high_resolution_clock::now();
        double batch_range_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(range_end_time -
                                                                     range_start_time)
                        .count();
        cumulative_range_time += batch_range_time;
        cumulative_ranges += num_range_per_batch;
    }

    long long cumulative_operations = cumulative_lookups + cumulative_ranges + cumulative_inserts;
    double cumulative_time = cumulative_lookup_time + cumulative_insert_time + (cumulative_ranges == 0 ? 0 : cumulative_range_time);
    std::cout << "index:ART"
              << " data_file:" << util::get_file_name(data_file)
              << " ns/lookup:"
              << cumulative_lookup_time / cumulative_lookups
              << " ns/range:"
              << cumulative_range_time / cumulative_ranges
              << " ns/insert:"
              << cumulative_insert_time / cumulative_inserts
              << " ns/op:"
              << cumulative_time / cumulative_operations
              << std::endl;
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
        case WorkloadType::READ_HEAVY: {
            ReadWriteBenchmark<uint64_t, uint64_t>(data_file, config);
            break;
        }
        case WorkloadType::SMALL_RANGE: {
            ReadWriteBenchmark<uint64_t, uint64_t>(data_file, config);
            break;
        }
        case WorkloadType::WRITE_HEAVY: {
            ReadWriteBenchmark<uint64_t, uint64_t>(data_file, config);
            break;
        }
        case WorkloadType::WRITE_ONLY: {
            ReadWriteBenchmark<uint64_t, uint64_t>(data_file, config);
            break;
        }
        case WorkloadType::READ_RANGE_WRITE: {
            ReadWriteBenchmark<uint64_t, uint64_t>(data_file, config);
            break;
        }
    }
    return 0;
}
