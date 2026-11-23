// Included after the existing synthetic world fixtures in tests.cpp.
WorldFixture* window_fixture;
const SegmentPoint* actual_window_point;
const unsigned* actual_window_direction;
unsigned fixture_wall_flags = 0xa1;
bool fixture_wall_attached = true;
unsigned window_common_calls = 0, window_wall_calls = 0;
bool __fastcall fake_xy_support(void*, void*, float x, float y) {
    SetLastError(7654);
    return x >= 0 && x < 20 && y >= 0 && y < 40;
}
void* __cdecl fixture_world() { return window_fixture; }
unsigned __cdecl fixture_flags(void*) { return fixture_wall_flags; }
bool __cdecl fixture_attached(void*, const SegmentPoint* point, const unsigned* direction, void*) {
    ++window_wall_calls;
    assert(point == actual_window_point && direction == actual_window_direction);
    return fixture_wall_attached;
}
void* __cdecl fixture_convert(void* destination, const SegmentPoint* point) {
    auto& result = *static_cast<Coordinate*>(destination);
    result = {static_cast<int32_t>(std::floor(point->x)), static_cast<int32_t>(std::floor(point->y)), point->level, 9};
    SetLastError(7654);
    return destination;
}
bool __cdecl fixture_common(void* object, const SegmentPoint* point, const unsigned* direction, void* errors) {
    ++window_common_calls;
    assert(object == window_fixture && point == actual_window_point && direction == actual_window_direction);
    assert(errors == reinterpret_cast<void*>(0x1234));
    const SegmentPoint before = *point;
    const unsigned original_direction = *direction;
    Coordinate tile{}, masked{};
    assert(support_tile_conversion(&tile, point) == &tile);
    assert(GetLastError() == 7654);
    assert(support_mask_conversion(&masked, point) == &masked && masked.mask == 9);
            const bool result = reinterpret_cast<bool (__thiscall*)(void*,float,float)>(&support_xy)(window_fixture, point->x, point->y);
    assert(GetLastError() == 7654);
    assert(!memcmp(&before, point, sizeof(before)) && original_direction == *direction);
    assert(tile.x == masked.x && tile.y == masked.y && tile.z == point->level);
    return result;
}
void test_window_support() {
    uintptr_t table[0x168/4]{};
    table[0x11c/4] = reinterpret_cast<uintptr_t>(&fake_tile);
    table[0x120/4] = reinterpret_cast<uintptr_t>(&fake_xy_support);
    table[0xe4/4] = reinterpret_cast<uintptr_t>(&fake_lock);
    table[0x144/4] = table[0x148/4] = reinterpret_cast<uintptr_t>(&fake_min);
    table[0x14c/4] = reinterpret_cast<uintptr_t>(&fake_max_x);
    table[0x150/4] = reinterpret_cast<uintptr_t>(&fake_max_y);
    WorldFixture fixture{table,{}};
    window_fixture = &fixture;
    native_world = fixture_world; native_flags = fixture_flags;
    native_common = fixture_common; native_wall_check = fixture_attached;
    native_tile_conversion = native_mask_conversion = fixture_convert;
    for (SegmentPoint outside : {SegmentPoint{-0.5f,5.5f,0},{20.5f,5.5f,0},{5.5f,-0.5f,0},{5.5f,40.5f,0}}) {
        for (unsigned direction : {0u,2u,4u,6u}) {
            actual_window_point = &outside; actual_window_direction = &direction;
            const SegmentPoint saved = outside;
            assert(window_common(&fixture, &outside, &direction, reinterpret_cast<void*>(0x1234)));
            assert(GetLastError() == 7654 && !window_support.active);
            assert(!memcmp(&saved, &outside, sizeof(saved)) && *actual_window_direction == direction);
            auto projected = make_window_support(fixture.model, outside, 0xa1, direction);
            assert(projected.active);
            WindowSupportScope scope(window_support, projected);
            Coordinate tile{static_cast<int32_t>(std::floor(outside.x)),static_cast<int32_t>(std::floor(outside.y)),outside.level,12};
            const Coordinate saved_tile = tile;
            assert(reinterpret_cast<bool (__thiscall*)(void*,const Coordinate*)>(&support_tile_bounds)(&fixture,&tile));
            assert(!memcmp(&saved_tile,&tile,sizeof(tile)));
            SegmentPoint inside{5.5f,5.5f,0}, copy{};
            assert(!projected.map_point(inside,copy));
            SegmentPoint wrong_level = outside; wrong_level.level=1;
            assert(!projected.map_point(wrong_level,copy));
            SegmentPoint corner{-0.5f,-0.5f,0};
            assert(!projected.map_point(corner,copy));
            auto inactive=make_window_support(fixture.model,outside,0, direction);
            { WindowSupportScope nested(window_support,inactive); assert(!window_support.active); }
            assert(window_support.active);
        }
    }
    assert(window_common_calls == 16 && window_wall_calls == 16);
    SegmentPoint outside{5.5f,40.5f,0}; unsigned direction=4;
    actual_window_point=&outside; actual_window_direction=&direction;
    fixture_wall_attached=false;
    assert(!window_common(&fixture,&outside,&direction,reinterpret_cast<void*>(0x1234)));
    assert(!window_support.active && GetLastError()==7654);
    fixture_wall_attached=true;
    fixture.model.locked=true;
    assert(!window_common(&fixture,&outside,&direction,reinterpret_cast<void*>(0x1234)));
    fixture.model.locked=false;
    for (unsigned d : {1u,3u,5u,7u,8u,0xffffffffu})
        assert(!make_window_support(fixture.model,outside,0xa1,d).active);
    for (unsigned flags : {0xa1u,0xa2u,0xa4u,0xa8u,0x8a1u,0x8a2u,0x8a4u,0x8a8u})
        assert(make_window_support(fixture.model,outside,flags,4).active);
    for (unsigned flags : {0u,1u,0xa0u,0xa3u,0xa5u,0xc0u,0x8a0u,0x8a3u,0x18a1u,0xffffu})
        assert(!make_window_support(fixture.model,outside,flags,4).active);
    for (SegmentPoint bad : {SegmentPoint{5.5f,41.5f,0},{-0.5f,-0.5f,0},{5.5f,40.25f,0},{5.5f,40.5f,-1},{5.5f,40.5f,2},{5.5f,NAN,0}})
        assert(!make_window_support(fixture.model,bad,0xa1,4).active);
    fixture_wall_flags=0;
    assert(!window_common(&fixture,&outside,&direction,reinterpret_cast<void*>(0x1234)));
    // Existing interior placement remains native and successful with no scope.
    SegmentPoint inside{5.5f,5.5f,0}; actual_window_point=&inside;
    assert(window_common(&fixture,&inside,&direction,reinterpret_cast<void*>(0x1234)));
    assert(!window_support.active && direction==4);
    puts("PASS: window support across four sides/four cardinal directions; actual point and direction preserved; x86 query ABIs, returns and LastError; locked/unsupported/malformed cases delegate; nested scopes restore.");
}
