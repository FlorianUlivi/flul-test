#ifndef FLUL_TEST_TEST_METADATA_HPP_
#define FLUL_TEST_TEST_METADATA_HPP_

#include <set>
#include <string_view>

namespace flul::test {

struct TestMetadata {
    std::string_view suite_name;
    std::string_view test_name;
    std::set<std::string_view> tags;

    [[nodiscard]] auto HasTag(std::string_view tag) const -> bool {
        return tags.contains(tag);
    }
};

}  // namespace flul::test

#endif  // FLUL_TEST_TEST_METADATA_HPP_
