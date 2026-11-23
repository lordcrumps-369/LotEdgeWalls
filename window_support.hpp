#pragma once
#include "boundary_policy.hpp"

inline bool window_component_flags(unsigned flags) {
    // Observed 0x0800 variant on the missing wide-window auxiliary. This is
    // eligibility only: original native flags are never cleared or replaced.
    return flags==0xa1 || flags==0xa2 || flags==0xa4 || flags==0xa8 ||
        flags==0x8a1 || flags==0x8a2 || flags==0x8a4 || flags==0x8a8;
}
inline bool window_group_flags(unsigned flags) {
    // 0xc0 was observed on a group root. It is admitted only with an actual
    // in-lot group anchor and full native wall validation, never standalone.
    return window_component_flags(flags) || flags==0xc0;
}

// A private query projection, never a change to the object's transform or world.
struct WindowSupport {
    bool active = false;
    int axis = 0;
    int delta = 0;
    int32_t min_x = 0, min_y = 0, max_x = 0, max_y = 0, level = 0;
    unsigned queries = 0;

    bool map_xy(float x, float y, float& out_x, float& out_y) const {
        if (!active || !std::isfinite(x) || !std::isfinite(y)) return false;
        const float a = axis == 0 ? x : y, b = axis == 0 ? y : x;
        const int32_t lo = axis == 0 ? min_x : min_y;
        const int32_t hi = axis == 0 ? max_x : max_y;
        const int32_t other_lo = axis == 0 ? min_y : min_x;
        const int32_t other_hi = axis == 0 ? max_y : max_x;
        if (!(b >= other_lo && b < static_cast<float>(other_hi) + 1)) return false;
        if (delta > 0) {
            if (!(a >= static_cast<float>(lo) - 1 && a < lo)) return false;
        } else if (!(a >= static_cast<float>(hi) + 1 && a < static_cast<float>(hi) + 2)) return false;
        out_x = x + (axis == 0 ? delta : 0);
        out_y = y + (axis == 1 ? delta : 0);
        return true;
    }
    bool map_point(const SegmentPoint& source, SegmentPoint& target) const {
        if (source.level != level) return false;
        target = source;
        return map_xy(source.x, source.y, target.x, target.y);
    }
    bool map_tile(const Coordinate& source, Coordinate& target) const {
        if (source.z != level) return false;
        float x = 0, y = 0;
        if (!map_xy(static_cast<float>(source.x), static_cast<float>(source.y), x, y)) return false;
        target = source;
        target.x = static_cast<int32_t>(x);
        target.y = static_cast<int32_t>(y);
        return true;
    }
};

template<class World>
WindowSupport make_window_support(World& world, const SegmentPoint& point, unsigned flags, unsigned direction) {
    WindowSupport result{};
    // Restrict standalone components to this cardinal-wall profile family.
    if (!window_component_flags(flags) || direction > 6 || (direction & 1) ||
        point.level < 0 || !std::isfinite(point.x) || !std::isfinite(point.y) ||
        std::fabs(point.x) > 1000000 || std::fabs(point.y) > 1000000 ||
        std::floor(point.x) + 0.5f != point.x || std::floor(point.y) + 0.5f != point.y) return result;
    result.min_x = world.bound(0x144); result.min_y = world.bound(0x148);
    result.max_x = world.bound(0x14c); result.max_y = world.bound(0x150);
    result.level = point.level;
    if (result.min_x >= result.max_x || result.min_y >= result.max_y ||
        std::abs(static_cast<int64_t>(result.min_x)) > 1000000 || std::abs(static_cast<int64_t>(result.min_y)) > 1000000 ||
        std::abs(static_cast<int64_t>(result.max_x)) > 1000000 || std::abs(static_cast<int64_t>(result.max_y)) > 1000000) return {};
    if (point.x == result.min_x - 0.5f) { result.axis = 0; result.delta = 1; }
    else if (point.x == result.max_x + 1.5f) { result.axis = 0; result.delta = -1; }
    else if (point.y == result.min_y - 0.5f) { result.axis = 1; result.delta = 1; }
    else if (point.y == result.max_y + 1.5f) { result.axis = 1; result.delta = -1; }
    else return {};
    result.active = true;
    SegmentPoint support{};
    if (!result.map_point(point, support)) return {};
    const Coordinate tile{static_cast<int32_t>(std::floor(support.x)), static_cast<int32_t>(std::floor(support.y)), support.level};
    if (!world.tile_valid(tile) || world.tile_locked(tile)) return {};
    return result;
}

// Nesting must restore the previous query context, including exception unwind.
struct WindowSupportScope {
    WindowSupport& current;
    WindowSupport previous;
    WindowSupportScope(WindowSupport& slot, WindowSupport value) : current(slot), previous(slot) { current = value; }
    ~WindowSupportScope() { current = previous; }
};
