#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace almond::voxel::test {

struct test_case {
    std::string name;
    std::function<void()> func;
};

inline std::vector<test_case>& registry() {
    static std::vector<test_case> tests;
    return tests;
}

inline void register_test(std::string name, std::function<void()> func) {
    registry().push_back(test_case{std::move(name), std::move(func)});
}

inline bool has_registered_tests() {
    return !registry().empty();
}

struct test_failure : std::exception {
    explicit test_failure(std::string message)
        : message_{std::move(message)} { }

    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

inline void run_tests() {
    std::size_t passed = 0;
    for (const auto& test : registry()) {
        try {
            test.func();
            ++passed;
        } catch (const test_failure& failure) {
            std::cerr << "[FAILED] " << test.name << " - " << failure.what() << '\n';
        } catch (const std::exception& ex) {
            std::cerr << "[FAILED] " << test.name << " - unexpected exception: " << ex.what() << '\n';
        } catch (...) {
            std::cerr << "[FAILED] " << test.name << " - unknown exception" << '\n';
        }
    }
    std::cout << "Executed " << registry().size() << " test(s)." << '\n';
    std::cout << "Passed   " << passed << " test(s)." << '\n';
    if (passed != registry().size()) {
        throw test_failure{"one or more tests failed"};
    }
}

} // namespace almond::voxel::test

#define ALMOND_CONCAT_IMPL(a, b) a##b
#define ALMOND_CONCAT(a, b) ALMOND_CONCAT_IMPL(a, b)

#define TEST_CASE(name)                                                                                                   \
    static void name();                                                                                                   \
    namespace {                                                                                                           \
    const bool ALMOND_CONCAT(name, _registered) = [] {                                                                    \
        ::almond::voxel::test::register_test(#name, &name);                                                               \
        return true;                                                                                                      \
    }();                                                                                                                  \
    }                                                                                                                     \
    static void name()

#define CHECK(expr)                                                                                                       \
    do {                                                                                                                 \
        if (!(expr)) {                                                                                                   \
            throw ::almond::voxel::test::test_failure{std::string{"CHECK failed: "} + #expr};                             \
        }                                                                                                                \
    } while (false)

#define CHECK_FALSE(expr) CHECK(!(expr))

#define REQUIRE(expr)                                                                                                     \
    do {                                                                                                                 \
        if (!(expr)) {                                                                                                   \
            throw ::almond::voxel::test::test_failure{std::string{"REQUIRE failed: "} + #expr};                           \
        }                                                                                                                \
    } while (false)

#define REQUIRE_FALSE(expr) REQUIRE(!(expr))

