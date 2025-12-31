#ifndef SCROLLER_HYPR_COMPAT_H
#define SCROLLER_HYPR_COMPAT_H

#include <hyprutils/math/Vector2D.hpp>

using Hyprutils::Math::Vector2D;

namespace hyprscroller_compat {

template<typename MON>
static inline auto _get_reserved_top_left_impl(MON monitor, int) -> decltype((void)monitor->m_reservedTopLeft, Vector2D{monitor->m_reservedTopLeft.x, monitor->m_reservedTopLeft.y}) {
	return Vector2D{monitor->m_reservedTopLeft.x, monitor->m_reservedTopLeft.y};
}
template<typename MON>
static inline auto _get_reserved_top_left_impl(MON monitor, long) -> decltype((void)monitor->m_reservedArea.topLeft, Vector2D{monitor->m_reservedArea.topLeft.x, monitor->m_reservedArea.topLeft.y}) {
	return Vector2D{monitor->m_reservedArea.topLeft.x, monitor->m_reservedArea.topLeft.y};
}
template<typename MON>
static inline Vector2D _get_reserved_top_left_impl(MON, ...) {
	return Vector2D{0.0, 0.0};
}

template<typename MON>
static inline Vector2D reserved_top_left(MON monitor) {
	return _get_reserved_top_left_impl(monitor, 0);
}

template<typename MON>
static inline auto _get_reserved_bottom_right_impl(MON monitor, int) -> decltype((void)monitor->m_reservedBottomRight, Vector2D{monitor->m_reservedBottomRight.x, monitor->m_reservedBottomRight.y}) {
	return Vector2D{monitor->m_reservedBottomRight.x, monitor->m_reservedBottomRight.y};
}
template<typename MON>
static inline auto _get_reserved_bottom_right_impl(MON monitor, long) -> decltype((void)monitor->m_reservedArea.bottomRight, Vector2D{monitor->m_reservedArea.bottomRight.x, monitor->m_reservedArea.bottomRight.y}) {
	return Vector2D{monitor->m_reservedArea.bottomRight.x, monitor->m_reservedArea.bottomRight.y};
}
template<typename MON>
static inline Vector2D _get_reserved_bottom_right_impl(MON, ...) {
	return Vector2D{0.0, 0.0};
}

template<typename MON>
static inline Vector2D reserved_bottom_right(MON monitor) {
	return _get_reserved_bottom_right_impl(monitor, 0);
}

} // namespace hyprscroller_compat

#endif // SCROLLER_HYPR_COMPAT_H