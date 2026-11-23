// Included inside plugin.cpp's anonymous namespace.
thread_local WindowSupport window_support;
thread_local WindowGroup window_group;
using PlacementCommon = bool (__cdecl*)(void*, const SegmentPoint*, const unsigned*, void*);
using WorldAccess = void* (__cdecl*)();
using FlagAccess = unsigned (__cdecl*)(void*);
using ConvertPoint = void* (__cdecl*)(void*, const SegmentPoint*);
PlacementCommon native_common = nullptr;
PlacementCommon native_wall_check = nullptr;
WorldAccess native_world = nullptr;
ConvertPoint native_tile_conversion = nullptr, native_mask_conversion = nullptr;
unsigned __cdecl wall_flags(void* object) {
    return method<unsigned (__thiscall*)(void*)>(object, 0x264)(object) & 0xffff;
}
FlagAccess native_flags = wall_flags;

const SegmentPoint* query_point(const SegmentPoint* point, SegmentPoint& support) {
    if (point && window_support.map_point(*point, support)) {
        ++window_support.queries;
        return &support;
    }
    return point;
}
bool __fastcall support_xy(void* self, void*, float x, float y) {
    float sx = x, sy = y;
    if (window_support.map_xy(x, y, sx, sy)) ++window_support.queries;
    return method<bool (__thiscall*)(void*, float, float)>(self, 0x120)(self, sx, sy);
}
bool __fastcall support_tile_bounds(void* self, void*, const Coordinate* tile) {
    Coordinate support{};
    if (tile && window_support.map_tile(*tile, support)) {
        ++window_support.queries;
        tile = &support;
    }
    return method<bool (__thiscall*)(void*, const Coordinate*)>(self, 0x11c)(self, tile);
}
void* __cdecl support_tile_conversion(void* destination, const SegmentPoint* point) {
    SegmentPoint support{};
    return native_tile_conversion(destination, query_point(point, support));
}
void* __cdecl support_mask_conversion(void* destination, const SegmentPoint* point) {
    SegmentPoint support{};
    return native_mask_conversion(destination, query_point(point, support));
}
bool __cdecl support_point_check(const SegmentPoint* point) {
    SegmentPoint support{};
    return game_function<bool (__cdecl*)(const SegmentPoint*)>(0xc24970)(query_point(point, support));
}
bool __cdecl support_point_flags(const SegmentPoint* point, unsigned flags) {
    SegmentPoint support{};
    return game_function<bool (__cdecl*)(const SegmentPoint*, unsigned)>(0xc24720)(query_point(point, support), flags);
}
bool __cdecl support_object_surface(void* object, const SegmentPoint* point) {
    SegmentPoint support{};
    return game_function<bool (__cdecl*)(void*, const SegmentPoint*)>(0xc28da0)(object, query_point(point, support));
}
bool __cdecl support_object_room(void* object, const SegmentPoint* point) {
    SegmentPoint support{};
    return game_function<bool (__cdecl*)(void*, const SegmentPoint*)>(0xc292b0)(object, query_point(point, support));
}
// Retain actual geometry and direction for 0xc294b0 and 0xc2e7e0. Their
// semantics include object/geometry constraints, not just support queries.

WindowGroup validated_window_group(void* object,const SegmentPoint* point,unsigned direction) {
    WindowGroup group{};
    if (!object || !point || direction>6 || (direction&1) || !window_group_flags(native_flags(object))) return group;
    if (!std::isfinite(point->x) || !std::isfinite(point->y) ||
        std::fabs(point->x)>1000000 || std::fabs(point->y)>1000000 ||
        std::floor(point->x)+0.5f!=point->x || std::floor(point->y)+0.5f!=point->y) return group;
    GameWorld world{native_world()};
    Coordinate tile{static_cast<int32_t>(std::floor(point->x)),static_cast<int32_t>(std::floor(point->y)),point->level};
    if (!world.self || !world.tile_valid(tile) || world.tile_locked(tile)) return group;
    if (!native_wall_check(object,point,&direction,nullptr)) return group;
    group.active=true;group.anchor=*point;group.direction=direction;return group;
}
WindowSupport component_support(GameWorld& world,const SegmentPoint& point,unsigned flags,unsigned direction) {
    auto support=make_window_support(world,point,flags,direction);
    if (!support.active && window_group.active && direction==window_group.direction) {
        auto candidate=make_window_support(world,point,0xa1,direction);
        if (window_group.covers(candidate,point)) support=candidate;
    }
    return support;
}
using GroupComponents = bool (__cdecl*)(void*,const SegmentPoint*,const unsigned*,float,float,unsigned,void*);
GroupComponents native_group_components=nullptr;
#include "group_entry.hpp"

bool __cdecl window_common(void* object, const SegmentPoint* point, const unsigned* direction, void* errors) {
    WindowSupport projected{};
    const unsigned orientation = direction ? *direction : 0xffffffffu;
    if (object && point && direction) {
        GameWorld world{native_world()};
        if (world.self) projected = component_support(world, *point, native_flags(object), orientation);
        // Use the full native wall validator at the real position. A failed
        // attachment cannot activate any support projection.
        if (projected.active && !native_wall_check(object, point, direction, nullptr)) projected = {};
    }
    WindowSupportScope scope(window_support, projected);
    const bool result = native_common(object, point, direction, errors);
    if (projected.active) {

    }
    return result;
}
