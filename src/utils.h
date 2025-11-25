#ifndef SCROLLER_UTILS_H
#define SCROLLER_UTILS_H

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

inline bool string_starts_with(std::string_view str, std::string_view prefix)
{
	return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

template<typename... Args>
std::string string_format(std::string_view format, Args&&... args)
{
	std::ostringstream stream;
	size_t position = 0;

	auto write_segment = [&](size_t placeholder) {
		stream << format.substr(position, placeholder - position);
		position = placeholder + 2;
	};

	auto append_argument = [&](auto&& value) {
		size_t placeholder = format.find("{}", position);
		if (placeholder == std::string_view::npos) {
			throw std::runtime_error("string_format: not enough placeholders");
		}
		write_segment(placeholder);
		stream << std::forward<decltype(value)>(value);
	};

	(append_argument(std::forward<Args>(args)), ...);

	if (position < format.size()) {
		stream << format.substr(position);
	}

	return stream.str();
}

#endif // SCROLLER_UTILS_H