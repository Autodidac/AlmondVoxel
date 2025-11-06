#include "test_framework.hpp"

#include <iostream>

int main() {
    try {
        almond::voxel::test::run_tests();
        return 0;
    } catch (const almond::voxel::test::test_failure& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
