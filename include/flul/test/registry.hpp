#ifndef FLUL_TEST_REGISTRY_HPP_
#define FLUL_TEST_REGISTRY_HPP_

#include <algorithm>
#include <concepts>
#include <format>
#include <initializer_list>
#include <iostream>
#include <print>
#include <set>
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
    auto Add(std::string_view suite_name, std::string_view test_name, void (S::*method)(),
             std::initializer_list<std::string_view> tags = {}) -> TestEntry& {
        std::set<std::string_view> unique_tags;
        for (std::string_view tag : tags) {
            if (!unique_tags.insert(tag).second) {
                std::println(std::cerr,
                             "[flul-test] warning: duplicate tag \"{}\" on test {}::{} -- ignoring",
                             tag, suite_name, test_name);
            }
        }
        entries_.push_back({
            TestMetadata{
                .suite_name = suite_name, .test_name = test_name, .tags = std::move(unique_tags)},
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
        return entries_.back();
    }

    [[nodiscard]] auto Tests() const -> std::span<const TestEntry> {
        return entries_;
    }

    void Filter(std::string_view pattern) {
        std::erase_if(entries_, [pattern](const TestEntry& e) {
            return !std::format("{}::{}", e.metadata.suite_name, e.metadata.test_name)
                        .contains(pattern);
        });
    }

    void FilterByTag(std::span<const std::string_view> include_tags) {
        if (include_tags.empty()) {
            return;
        }
        std::erase_if(entries_, [include_tags](const TestEntry& e) {
            return !std::ranges::any_of(
                include_tags, [&e](std::string_view tag) { return e.metadata.HasTag(tag); });
        });
    }

    void ExcludeByTag(std::span<const std::string_view> exclude_tags) {
        if (exclude_tags.empty()) {
            return;
        }
        std::erase_if(entries_, [exclude_tags](const TestEntry& e) {
            return std::ranges::any_of(
                exclude_tags, [&e](std::string_view tag) { return e.metadata.HasTag(tag); });
        });
    }

    void List() const {
        for (const auto& e : entries_) {
            std::println("{}::{}", e.metadata.suite_name, e.metadata.test_name);
        }
    }

    void ListVerbose() const {
        for (const auto& e : entries_) {
            if (e.metadata.tags.empty()) {
                std::println("{}::{}", e.metadata.suite_name, e.metadata.test_name);
            } else {
                std::string tag_str;
                bool first = true;
                for (const auto& tag : e.metadata.tags) {
                    if (!first) {
                        tag_str += ", ";
                    }
                    tag_str += tag;
                    first = false;
                }
                std::println("{}::{} [{}]", e.metadata.suite_name, e.metadata.test_name, tag_str);
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
