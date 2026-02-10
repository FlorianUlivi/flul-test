#pragma once

#include <concepts>
#include <format>
#include <sstream>
#include <string>

#include <cxxabi.h>

namespace flul::test {

auto Demangle(const char* mangled) -> std::string;

// (1) Primary: types with std::format support
template <typename T>
    requires std::formattable<T, char>
auto Stringify(const T& value) -> std::string {
    return std::format("{}", value);
}

// (2) Fallback: types with operator<<
template <typename T>
    requires(!std::formattable<T, char>) && requires(std::ostream& os, const T& v) { os << v; }
auto Stringify(const T& value) -> std::string {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// (3) Final fallback: non-printable types
template <typename T>
    requires(!std::formattable<T, char>) && (!requires(std::ostream& os, const T& v) { os << v; })
auto Stringify(const T& /*value*/) -> std::string {
    return "<non-printable>";
}

inline auto Demangle(const char* mangled) -> std::string {
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    if (status == 0 && demangled != nullptr) {
        std::string result(demangled);
        free(demangled);  // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        return result;
    }
    return mangled;
}

}  // namespace flul::test
