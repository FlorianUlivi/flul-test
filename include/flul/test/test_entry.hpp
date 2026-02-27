#ifndef FLUL_TEST_TEST_ENTRY_HPP_
#define FLUL_TEST_TEST_ENTRY_HPP_

#include <algorithm>
#include <functional>
#include <string_view>
#include <vector>

namespace flul::test {

struct TestEntry {
    std::string_view suite_name;
    std::string_view test_name;
    std::function<void()> callable;
    std::vector<std::string_view> tags;

    [[nodiscard]] auto HasTag(std::string_view tag) const -> bool {
        return std::ranges::find(tags, tag) != tags.end();
    }
};

}  // namespace flul::test

#endif  // FLUL_TEST_TEST_ENTRY_HPP_
