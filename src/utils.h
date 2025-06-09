#ifndef SCROLLER_UTILS_H
#define SCROLLER_UTILS_H

#include <string>
#include <sstream>

// Utility functions for compatibility with older C++ standards

template<typename... Args>
inline std::string string_format(const std::string& format, Args... args) {
    std::ostringstream oss;
    size_t pos = 0;
    size_t arg_index = 0;
    
    auto format_arg = [&](auto&& arg) {
        if (arg_index == 0 && pos < format.size()) {
            size_t start = format.find("{}", pos);
            if (start != std::string::npos) {
                oss << format.substr(pos, start - pos) << arg;
                pos = start + 2;
                arg_index++;
            }
        } else if (pos < format.size()) {
            size_t start = format.find("{}", pos);
            if (start != std::string::npos) {
                oss << format.substr(pos, start - pos) << arg;
                pos = start + 2;
                arg_index++;
            }
        }
    };
    
    (format_arg(args), ...);
    if (pos < format.size()) {
        oss << format.substr(pos);
    }
    return oss.str();
}

inline bool string_starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

#endif // SCROLLER_UTILS_H
