#ifndef FLUL_TEST_TEST_ENTRY_HPP_
#define FLUL_TEST_TEST_ENTRY_HPP_

#include <functional>
#include <set>
#include <string_view>

namespace flul::test {

struct TestEntry {
    std::string_view suite_name;
    std::string_view test_name;
    std::function<void()> callable;
    std::set<std::string_view> tags;

    [[nodiscard]] auto HasTag(std::string_view tag) const -> bool {
        return tags.contains(tag);
    }
};

}  // namespace flul::test

#endif  // FLUL_TEST_TEST_ENTRY_HPP_
