#ifndef FLUL_TEST_SUITE_HPP_
#define FLUL_TEST_SUITE_HPP_

#include <initializer_list>
#include <string_view>
#include <utility>

namespace flul::test {

class Registry;  // forward declaration — full definition in registry.hpp

template <typename Derived>
class Suite {
   public:
    virtual ~Suite() = default;
    virtual void SetUp() {}
    virtual void TearDown() {}

    Suite(const Suite&) = delete;
    auto operator=(const Suite&) -> Suite& = delete;
    Suite(Suite&&) = delete;
    auto operator=(Suite&&) -> Suite& = delete;

    // Convenience bulk-registration helper.
    // Definition is in registry.hpp, after Registry is fully defined.
    static void AddTests(
        Registry& r, std::string_view suite_name,
        std::initializer_list<std::pair<std::string_view, void (Derived::*)()>> tests);

   protected:
    Suite() = default;
};

}  // namespace flul::test

#endif  // FLUL_TEST_SUITE_HPP_
