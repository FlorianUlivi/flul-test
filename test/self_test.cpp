#include "flul/test/registry.hpp"
#include "flul/test/run.hpp"

namespace assertion_error_test {
void Register(flul::test::Registry& r);
}
namespace stringify_test {
void Register(flul::test::Registry& r);
}
namespace expect_test {
void Register(flul::test::Registry& r);
}
namespace expect_callable_test {
void Register(flul::test::Registry& r);
}
namespace registry_test {
void Register(flul::test::Registry& r);
}
namespace runner_test {
void Register(flul::test::Registry& r);
}
namespace run_test {
void Register(flul::test::Registry& r);
}
namespace fixture_test {
void Register(flul::test::Registry& r);
}

auto main(int argc, char* argv[]) -> int {
    flul::test::Registry registry;

    assertion_error_test::Register(registry);
    stringify_test::Register(registry);
    expect_test::Register(registry);
    expect_callable_test::Register(registry);
    registry_test::Register(registry);
    runner_test::Register(registry);
    run_test::Register(registry);
    fixture_test::Register(registry);

    return flul::test::Run(argc, argv, registry);
}
