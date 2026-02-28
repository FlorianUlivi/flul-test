#ifndef FLUL_TEST_TEST_ENTRY_HPP_
#define FLUL_TEST_TEST_ENTRY_HPP_

#include <functional>

#include "flul/test/test_metadata.hpp"

namespace flul::test {

struct TestEntry {
    TestMetadata metadata;
    std::function<void()> callable;
};

}  // namespace flul::test

#endif  // FLUL_TEST_TEST_ENTRY_HPP_
