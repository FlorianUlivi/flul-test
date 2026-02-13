#ifndef FLUL_TEST_RUN_HPP_
#define FLUL_TEST_RUN_HPP_

#include <print>
#include <string_view>

#include "flul/test/registry.hpp"
#include "flul/test/runner.hpp"

namespace flul::test {

inline auto Run(int argc, char* argv[], Registry& registry) -> int {
    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);

        if (arg == "--list") {
            registry.List();
            return 0;
        }
        if (arg == "--filter") {
            if (i + 1 >= argc) {
                std::println(stderr, "error: --filter requires an argument");
                return 1;
            }
            registry.Filter(argv[++i]);
        } else if (arg == "--help") {
            std::println("usage: {} [--list] [--filter <pattern>] [--help]", argv[0]);
            return 0;
        } else {
            std::println(stderr, "error: unknown option '{}'", arg);
            std::println(stderr, "usage: {} [--list] [--filter <pattern>] [--help]", argv[0]);
            return 1;
        }
    }

    Runner runner(registry);
    return runner.RunAll();
}

}  // namespace flul::test

#endif  // FLUL_TEST_RUN_HPP_
