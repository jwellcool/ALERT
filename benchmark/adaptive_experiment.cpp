//
// Created by Cshuang on 2020/12/8.
//

#include <iostream>
#include <map>
#include <chrono>
#include <thread>
#include <algorithm>
#include "alert-index.h"
#include "util.h"
using namespace std;

const int MAX_ERROR      = 32;
const int TOTAL_NUM_OPS  = 10000000;
const int TOTAL_BATCH_NO = 200;

const int SAMPLE_GAP = 100;

template<typename KeyType, typename ValueType>
void PointLookup(const string data_file) {
    // Load data
    vector<KeyType> keys = util::load_data<KeyType>(data_file);

    vector<KeyType> init_keys, insert_keys;

    util::sample_keys(keys, keys.size(), init_keys, insert_keys, keys.size()/SAMPLE_GAP);

    auto init_values = util::make_values<KeyType, ValueType>(init_keys);

    // Create and bulk load
    alert::Alert<KeyType, ValueType> index(MAX_ERROR);
    index.BulkLoad(init_keys, init_values);

    for (size_t i = 0; i < insert_keys.size(); i++) {
        // Perform operation
        index.Insert(insert_keys[i], insert_keys[i]);
    }

    // Run workload
    const int num_lookups_per_batch = TOTAL_NUM_OPS / TOTAL_BATCH_NO;
    int batch_no = 0;
    cout << "batch_no,ns-lookup" << endl;
    while (batch_no < TOTAL_BATCH_NO) {
        // Do lookups
        KeyType* lookup_keys = nullptr;

        lookup_keys = util::get_search_keys_zipf(insert_keys, insert_keys.size(), num_lookups_per_batch, batch_no);

        auto lookups_start_time = std::chrono::high_resolution_clock::now();
        ValueType v;
        for (int j = 0; j < num_lookups_per_batch; j++) {
            // Perform operation
            index.Find(lookup_keys[j], v);
        }
        auto lookups_end_time = std::chrono::high_resolution_clock::now();
        double batch_lookup_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(lookups_end_time -
                                                                     lookups_start_time)
                        .count();

        batch_no++;
        cout << batch_no << ","
             << batch_lookup_time / num_lookups_per_batch << ","
             << endl;

        delete[] lookup_keys;
    }
}

template<typename KeyType, typename ValueType>
void Range(const string data_file) {
    // Load data
    vector<KeyType> keys = util::load_data<KeyType>(data_file);

    vector<KeyType> init_keys, insert_keys;

    util::sample_keys(keys, keys.size(), init_keys, insert_keys, keys.size()/SAMPLE_GAP);

    auto init_values = util::make_values<KeyType, ValueType>(init_keys);

    // Create and bulk load
    alert::Alert<KeyType, ValueType> index(MAX_ERROR);
    index.BulkLoad(init_keys, init_values);

    for (size_t i = 0; i < insert_keys.size(); i++) {
        // Perform operation
        index.Insert(insert_keys[i], i);
    }

    const int max_range = 1000;

    // Run workload
    const int num_range_per_batch = TOTAL_NUM_OPS / TOTAL_BATCH_NO;
    int batch_no = 0;
    cout << "batch_no,ns-range" << endl;
    while (batch_no < TOTAL_BATCH_NO) {

        // Do range
        vector<RangeLookup<KeyType>> range_lookup = util::generate_range_lookups<KeyType>(keys, keys.size(), num_range_per_batch,
                                                                                          max_range, "zipf", batch_no);

        auto range_start_time = std::chrono::high_resolution_clock::now();
        for (int j = 0; j < num_range_per_batch; j++) {
            // Perform operation
            std::vector<std::pair<KeyType, uint64_t>> kvs;
            kvs.reserve(max_range+1);
            index.Range(range_lookup[j].start, range_lookup[j].end, kvs);
        }
        auto range_end_time = std::chrono::high_resolution_clock::now();
        double batch_range_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(range_end_time -
                                                                             range_start_time)
                        .count();
        batch_no++;
        cout << batch_no << "," << batch_range_time / num_range_per_batch <<  endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "usage: " << argv[0] << " <data_file> " << argv[1] << " <type>" << endl;
        throw;
    }
    const string data_file = argv[1];
    const string type = argv[2];

    util::set_cpu_affinity(0);

    if (type == "point")
        PointLookup<uint64_t, uint64_t>(data_file);
    else if (type == "range")
        Range<uint64_t, uint64_t>(data_file);
    else
        cerr << "error type, either point or range." << endl;

    return 0;
}
