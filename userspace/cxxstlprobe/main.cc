#include <map>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <utility>
#include <vector>

struct Region {
    uintptr_t begin;
    uintptr_t end;
};

struct AddressEndOrder {
    bool operator()(const Region *left, const Region *right) const {
        if (left->end != right->end) {
            return left->end < right->end;
        }
        return left->begin < right->begin;
    }
};

static int check_region_set() {
    Region first{0x1000, 0x2000};
    Region second{0x3000, 0x5000};
    Region third{0x2000, 0x3000};
    std::set<Region *, AddressEndOrder> regions;
    regions.insert(&second);
    regions.insert(&first);
    regions.insert(&third);

    uintptr_t expected_ends[] = {0x2000, 0x3000, 0x5000};
    size_t index = 0;
    for (Region *region : regions) {
        if (index >= sizeof(expected_ends) / sizeof(expected_ends[0])) {
            return 10;
        }
        if (region->end != expected_ends[index]) {
            return 11;
        }
        index++;
    }
    if (index != sizeof(expected_ends) / sizeof(expected_ends[0])) {
        return 12;
    }

    regions.erase(&third);
    if (regions.size() != 2) {
        return 13;
    }
    return 0;
}

static int check_malloc_alignment() {
    for (size_t size = 1; size <= 4096; size = size * 3 + 1) {
        void *ptr = malloc(size);
        if (ptr == nullptr) {
            return 40;
        }
        uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
        free(ptr);
        if ((address & 15u) != 0) {
            return 41;
        }
    }
    return 0;
}

static int check_vector() {
    std::vector<int> values;
    for (int i = 0; i < 16; i++) {
        values.push_back(i * 3);
    }
    int sum = 0;
    for (int value : values) {
        sum += value;
    }
    if (values.size() != 16 || values.capacity() < values.size()) {
        return 20;
    }
    if (sum != 360) {
        return 21;
    }
    values.erase(values.begin() + 4, values.begin() + 8);
    if (values.size() != 12 || values[4] != 24) {
        return 22;
    }
    return 0;
}

static int check_map() {
    std::map<int, int> squares;
    for (int i = 1; i <= 8; i++) {
        squares.insert(std::make_pair(i, i * i));
    }
    if (squares.size() != 8 || squares[7] != 49) {
        return 30;
    }
    squares.erase(3);
    if (squares.find(3) != squares.end()) {
        return 31;
    }
    return 0;
}

extern "C" int main() {
    printf("cxxstlprobe: start\n");
    printf("cxxstlprobe: sizeof(set<Region*>)=%llu\n",
        (unsigned long long)sizeof(std::set<Region *, AddressEndOrder>));

    int status = check_malloc_alignment();
    if (status != 0) {
        printf("cxxstlprobe: malloc alignment failed %d\n", status);
        return status;
    }

    status = check_region_set();
    if (status != 0) {
        printf("cxxstlprobe: region set failed %d\n", status);
        return status;
    }

    status = check_vector();
    if (status != 0) {
        printf("cxxstlprobe: vector failed %d\n", status);
        return status;
    }

    status = check_map();
    if (status != 0) {
        printf("cxxstlprobe: map failed %d\n", status);
        return status;
    }

    printf("cxxstlprobe: ok\n");
    return 0;
}
