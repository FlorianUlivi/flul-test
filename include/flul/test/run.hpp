#ifndef FLUL_TEST_RUN_HPP_
#define FLUL_TEST_RUN_HPP_

#include <print>
#include <string_view>
#include <vector>

#include "flul/test/registry.hpp"
#include "flul/test/runner.hpp"

namespace flul::test {

inline auto Run(int argc, char* argv[], Registry& registry) -> int {
    std::string_view filter_pattern;
    bool has_filter = false;
    bool do_list = false;
    bool do_list_verbose = false;
    std::vector<std::string_view> include_tags;
    std::vector<std::string_view> exclude_tags;

    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);

        if (arg == "--list") {
            do_list = true;
        } else if (arg == "--list-verbose") {
            do_list_verbose = true;
        } else if (arg == "--filter") {
            if (i + 1 >= argc) {
                std::println(stderr, "error: --filter requires an argument");
                return 1;
            }
            filter_pattern = argv[++i];
            has_filter = true;
        } else if (arg == "--tag") {
            if (i + 1 >= argc) {
                std::println(stderr, "error: --tag requires an argument");
                return 1;
            }
            include_tags.emplace_back(argv[++i]);
        } else if (arg == "--exclude-tag") {
            if (i + 1 >= argc) {
                std::println(stderr, "error: --exclude-tag requires an argument");
                return 1;
            }
            exclude_tags.emplace_back(argv[++i]);
        } else if (arg == "--help") {
            std::println(
                "usage: {} [--list] [--list-verbose] [--filter <pattern>] "
                "[--tag <tag>] [--exclude-tag <tag>] [--help]",
                argv[0]);
            return 0;
        } else {
            std::println(stderr, "error: unknown option '{}'", arg);
            std::println(stderr,
                         "usage: {} [--list] [--list-verbose] [--filter <pattern>] "
                         "[--tag <tag>] [--exclude-tag <tag>] [--help]",
                         argv[0]);
            return 1;
        }
    }

    if (has_filter) {
        registry.Filter(filter_pattern);
    }
    registry.FilterByTag(include_tags);
    registry.ExcludeByTag(exclude_tags);

    if (do_list) {
        registry.List();
        return 0;
    }
    if (do_list_verbose) {
        registry.ListVerbose();
        return 0;
    }

    Runner runner(registry);
    return runner.RunAll();
}

}  // namespace flul::test

#endif  // FLUL_TEST_RUN_HPP_
