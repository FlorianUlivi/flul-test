#ifndef FLUL_TEST_REGISTRY_HPP_
#define FLUL_TEST_REGISTRY_HPP_

#include <algorithm>
#include <concepts>
#include <format>
#include <initializer_list>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "flul/test/suite.hpp"
#include "flul/test/test_entry.hpp"

namespace flul::test {

class Registry {
   public:
    template <typename S>
        requires std::derived_from<S, Suite<S>> && std::default_initializable<S>
    void Add(std::string_view suite_name, std::string_view test_name, void (S::*method)()) {
        entries_.push_back({
            suite_name,
            test_name,
            [method]() {
                S instance;
                instance.SetUp();
                try {
                    (instance.*method)();
                } catch (...) {
                    instance.TearDown();
                    throw;
                }
                instance.TearDown();
            },
        });
    }

    [[nodiscard]] auto Tests() const -> std::span<const TestEntry> {
        return entries_;
    }

    void Filter(std::string_view pattern) {
        std::erase_if(entries_, [pattern](const TestEntry& e) {
            return !std::format("{}::{}", e.suite_name, e.test_name).contains(pattern);
        });
    }

    void List() const {
        for (const auto& e : entries_) {
            std::println("{}::{}", e.suite_name, e.test_name);
        }
    }

   private:
    std::vector<TestEntry> entries_;
};

// Out-of-line definition of Suite<Derived>::AddTests.
// Lives here because it requires Registry to be fully defined.
template <typename Derived>
void Suite<Derived>::AddTests(
    Registry& r, std::string_view suite_name,
    std::initializer_list<std::pair<std::string_view, void (Derived::*)()>> tests) {
    for (const auto& [name, method] : tests) {
        r.Add<Derived>(suite_name, name, method);
    }
}

}  // namespace flul::test

#endif  // FLUL_TEST_REGISTRY_HPP_
