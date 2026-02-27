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
    void Add(std::string_view suite_name, std::string_view test_name, void (S::*method)(),
             std::initializer_list<std::string_view> tags = {}) {
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
            std::vector<std::string_view>(tags),
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

    void FilterByTag(std::span<const std::string_view> include_tags) {
        if (include_tags.empty()) {
            return;
        }
        std::erase_if(entries_, [include_tags](const TestEntry& e) {
            return !std::ranges::any_of(include_tags,
                                        [&e](std::string_view tag) { return e.HasTag(tag); });
        });
    }

    void ExcludeByTag(std::span<const std::string_view> exclude_tags) {
        if (exclude_tags.empty()) {
            return;
        }
        std::erase_if(entries_, [exclude_tags](const TestEntry& e) {
            return std::ranges::any_of(exclude_tags,
                                       [&e](std::string_view tag) { return e.HasTag(tag); });
        });
    }

    void List() const {
        for (const auto& e : entries_) {
            std::println("{}::{}", e.suite_name, e.test_name);
        }
    }

    void ListVerbose() const {
        for (const auto& e : entries_) {
            if (e.tags.empty()) {
                std::println("{}::{}", e.suite_name, e.test_name);
            } else {
                std::string tag_str;
                for (std::size_t i = 0; i < e.tags.size(); ++i) {
                    if (i > 0) {
                        tag_str += ", ";
                    }
                    tag_str += e.tags[i];
                }
                std::println("{}::{} [{}]", e.suite_name, e.test_name, tag_str);
            }
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
    std::initializer_list<std::pair<std::string_view, void (Derived::*)()>> tests,
    std::initializer_list<std::string_view> tags) {
    for (const auto& [name, method] : tests) {
        r.Add<Derived>(suite_name, name, method, tags);
    }
}

}  // namespace flul::test

#endif  // FLUL_TEST_REGISTRY_HPP_
