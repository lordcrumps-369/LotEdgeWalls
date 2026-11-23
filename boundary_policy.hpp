#pragma once
#include <cstdint>
#include <limits>
#include <cmath>

struct Coordinate { int32_t x, y, z; unsigned char mask = 0; };
struct SegmentPoint { float x, y; int32_t level; };
struct Segment { SegmentPoint first, second; };

template<class World>
bool foundation_corner_valid(World& world, float x, float y) {
    if (!(x >= -1000000 && x <= 1000000 && y >= -1000000 && y <= 1000000) ||
        std::floor(x) != x || std::floor(y) != y) return false;
    return world.vertex_valid(Coordinate{static_cast<int32_t>(x), static_cast<int32_t>(y), 0});
}

template<class World>
bool foundation_corner_locked(World& world, const Coordinate& corner) {
    if (world.tile_valid(corner)) return world.tile_locked(corner);
    // A valid far-side corner is a vertex, not a valid tile-array index.
    // Any coordinate that is neither a real tile nor a ground vertex fails closed.
    if (corner.z != 0 || !world.vertex_valid(corner)) return true;
    const auto max_x = world.bound(0x14c), max_y = world.bound(0x150);
    if (max_x == std::numeric_limits<int32_t>::max() || max_y == std::numeric_limits<int32_t>::max()) return true;
    return corner.x != max_x + 1 && corner.y != max_y + 1;
}

template<class World>
bool boundary_segment(World& world, const Segment& segment) {
    const auto& a = segment.first;
    const auto& b = segment.second;
    if (a.level < 0 || a.level != b.level) return false;
    const int32_t x0 = world.bound(0x144), y0 = world.bound(0x148);
    const int32_t x1 = world.bound(0x14c), y1 = world.bound(0x150);
    if (x0 >= x1 || y0 >= y1 || x1 == std::numeric_limits<int32_t>::max() || y1 == std::numeric_limits<int32_t>::max()) return false;
    const float min_x = static_cast<float>(x0), min_y = static_cast<float>(y0);
    const float max_x = static_cast<float>(x1 + 1), max_y = static_cast<float>(y1 + 1);
    if (!(a.x >= min_x && a.x <= max_x && b.x >= min_x && b.x <= max_x &&
          a.y >= min_y && a.y <= max_y && b.y >= min_y && b.y <= max_y)) return false;
    return (a.x == b.x && a.y != b.y && (a.x == min_x || a.x == max_x)) ||
           (a.y == b.y && a.x != b.x && (a.y == min_y || a.y == max_y));
}

template<class World>
bool boundary_wall_endpoints_valid(World& world, const Segment& segment) {
    if (!boundary_segment(world, segment)) return false;
    const SegmentPoint* points[] = {&segment.first, &segment.second};
    for (const SegmentPoint* point : points) {
        if (!(point->x >= -1000000 && point->x <= 1000000 && point->y >= -1000000 && point->y <= 1000000) ||
            std::floor(point->x) != point->x || std::floor(point->y) != point->y) return false;
        Coordinate vertex{static_cast<int32_t>(point->x), static_cast<int32_t>(point->y), point->level};
        if (!world.vertex_valid(vertex)) return false;
    }
    return true;
}

template<class World>
bool unlocked_boundary_vertex(World& world, const Coordinate& vertex, bool use_ignore_option = true) {
    // Allocation-aware validation applies to both base and cap vertices.
    if (vertex.z < 0 || !world.vertex_valid(vertex)) return false;
    const auto min_x = world.bound(0x144);
    const auto min_y = world.bound(0x148);
    const auto max_x = world.bound(0x14c);
    const auto max_y = world.bound(0x150);
    if (min_x >= max_x || min_y >= max_y ||
        min_x == std::numeric_limits<int32_t>::min() || min_y == std::numeric_limits<int32_t>::min() ||
        max_x == std::numeric_limits<int32_t>::max() || max_y == std::numeric_limits<int32_t>::max()) return false;
    if (vertex.x != min_x && vertex.y != min_y && vertex.x != max_x + 1 && vertex.y != max_y + 1) return false;
    const bool ignore_locks = use_ignore_option && world.ignore_locks();
    constexpr int offsets[4][2] = {{-1,-1},{0,-1},{0,0},{-1,0}};
    constexpr unsigned char masks[4] = {6,3,9,12};
    unsigned present = 0;
    for (unsigned i = 0; i < 4; ++i) {
        Coordinate tile{vertex.x + offsets[i][0], vertex.y + offsets[i][1], vertex.z, masks[i]};
        if (!world.tile_valid(tile)) continue;
        ++present;
        if (!ignore_locks && world.tile_locked(tile)) return false;
    }
    return present != 0;
}

template<class World>
bool roof_boundary_corner_valid(World& world, const SegmentPoint& point) {
    if (!(point.x >= -1000000 && point.x <= 1000000 && point.y >= -1000000 && point.y <= 1000000) ||
        point.level < 0 || std::floor(point.x) != point.x || std::floor(point.y) != point.y) return false;
    Coordinate vertex{static_cast<int32_t>(point.x), static_cast<int32_t>(point.y), point.level};
    // The roof validator always checks existing tile locks, even if other
    // tools are configured to ignore them. Preserve that stricter behavior.
    return unlocked_boundary_vertex(world, vertex, false);
}
