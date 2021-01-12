#include <iostream>
#include <vector>
#include <unordered_set>
#include <random>
#include <chrono>
#include "alert-index.h"
using namespace std;

void Example() {
    // Create random keys.
    vector<uint64_t> keys, values;

    for (int i = 0; i < 1000000; i+=10000) {
        keys.push_back(i);
        values.push_back(i);
    }

    alert::Alert<uint64_t, uint64_t> index;
    index.BulkLoad(keys, values);

    for (int i = 0; i < 1000000; i++) {
        index.Insert(i, i);
    }

    cout << "******" << endl;

    auto lookups_start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i+=50) {
        uint64_t v;
        index.Find(i, v);
    }
    auto lookups_end_time = std::chrono::high_resolution_clock::now();

    cout << chrono::duration_cast<chrono::nanoseconds>(lookups_end_time - lookups_start_time).count() << endl;

    cout << "******" << endl;
    lookups_start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i+=50) {
        uint64_t v;
        index.Find(i, v);
    }
    lookups_end_time = std::chrono::high_resolution_clock::now();

    cout << chrono::duration_cast<chrono::nanoseconds>(lookups_end_time - lookups_start_time).count() << endl;
}


int main(int argc, char** argv) {
    Example();
    return 0;
}
