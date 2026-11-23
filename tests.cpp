#include <cstdio>
#include "plugin.cpp"
#include <cassert>
#include <initializer_list>

struct FakeWorld {
    bool locked = false, ignore = false;
    int maximum_level = 1;
    unsigned tile_queries = 0, lock_queries = 0;
    bool vertex_valid(const Coordinate& c) { return c.x >= 0 && c.x <= 20 && c.y >= 0 && c.y <= 40 && c.z >= 0 && c.z <= maximum_level; }
    bool tile_valid(const Coordinate& c) { ++tile_queries; return c.x >= 0 && c.x < 20 && c.y >= 0 && c.y < 40 && c.z >= 0 && c.z <= maximum_level; }
    bool tile_locked(const Coordinate& c) {
        assert(c.x >= 0 && c.x < 20 && c.y >= 0 && c.y < 40 && c.z >= 0 && c.z <= maximum_level);
        ++lock_queries;
        return locked;
    }
    bool ignore_locks() { return ignore; }
    int32_t bound(unsigned offset) { return offset == 0x14c ? 19 : offset == 0x150 ? 39 : 0; }
};
struct WorldFixture { uintptr_t* vtable; FakeWorld model; };
bool __fastcall fake_vertex(void* self, void*, const Coordinate* c) { return static_cast<WorldFixture*>(self)->model.vertex_valid(*c); }
bool __fastcall fake_tile(void* self, void*, const Coordinate* c) { return static_cast<WorldFixture*>(self)->model.tile_valid(*c); }
bool __fastcall fake_lock(void* self, void*, const Coordinate* c) { return static_cast<WorldFixture*>(self)->model.tile_locked(*c); }
int32_t __fastcall fake_min(void*, void*) { return 0; }
int32_t __fastcall fake_max_x(void*, void*) { return 19; }
int32_t __fastcall fake_max_y(void*, void*) { return 39; }
const Segment* expected_segment;
bool expected_result;
unsigned thunk_calls = 0;
bool __cdecl fake_side_adapter(const void* partition, const Segment* segment) {
    assert(partition == reinterpret_cast<void*>(0x1234));
    assert(segment == expected_segment);
    ++thunk_calls;
    return expected_result;
}
__declspec(naked) bool __cdecl invoke_side_thunk(const void*, const Segment*) {
    __asm {
        push ebx
        mov eax, dword ptr [esp+8]
        mov ebx, dword ptr [esp+12]
        push eax
        call single_side_thunk
        add esp, 4
        pop ebx
        ret
    }
}
#include "window_tests.hpp"
#include "registration_tests.hpp"
#include "assignment_tests.hpp"
int main() {
    test_window_support();
    test_registration();
    test_assignment_scope();
    for (Coordinate c : {Coordinate{0,20,0}, {20,20,0}, {10,0,0}, {10,40,0}, {0,0,0}, {20,40,0}, {0,20,1}}) {
        FakeWorld world;
        assert(unlocked_boundary_vertex(world, c));
        assert(world.tile_queries == 4 && world.lock_queries >= 1 && world.lock_queries <= 2);
        world.locked = true;
        assert(!unlocked_boundary_vertex(world, c));
        world.ignore = true;
        world.lock_queries = 0;
        assert(unlocked_boundary_vertex(world, c));
        assert(world.lock_queries == 0);
    }
    for (Coordinate c : {Coordinate{10,20,0}, {-1,20,0}, {21,20,0}, {0,-1,0}, {0,41,0}, {0,20,2}, {0,20,-1}}) {
        FakeWorld world;
        assert(!unlocked_boundary_vertex(world, c));
        assert(world.tile_queries == 0 && world.lock_queries == 0);
    }
    auto* memory = static_cast<BYTE*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(memory);
    memset(memory, 0xcc, 4096);
    memcpy(memory, strict_call, sizeof(strict_call));
    memcpy(memory + 18, strict_call, sizeof(strict_call));
    BYTE original[4096];
    memcpy(original, memory, 4096);
    DWORD previous = 0;
    assert(VirtualProtect(memory, 4096, PAGE_EXECUTE_READ, &previous));
    assert(patch_calls(memory, memory + 18));
    for (unsigned i = 0; i < 4096; ++i) assert(memory[i] == ((i == 2 || i == 20) ? 0x18 : original[i]));
    MEMORY_BASIC_INFORMATION info{};
    assert(VirtualQuery(memory, &info, sizeof(info)));
    assert(info.Protect == PAGE_EXECUTE_READ);
    assert(!patch_calls(memory, memory + 18));
    assert(!patch_calls(memory + 18, memory));
    assert(!patch_calls(memory, memory + 128));
    assert(VirtualProtect(memory, 4096, PAGE_READWRITE, &previous));
    memcpy(memory, strict_call, sizeof(strict_call));
    memcpy(memory + 18, strict_call, sizeof(strict_call));
    memory[19] = 0x91;
    assert(!patch_calls(memory, memory + 18));
    assert(!memcmp(memory, strict_call, sizeof(strict_call)));
    assert(VirtualFree(memory, 0, MEM_RELEASE));
    auto* pages = static_cast<BYTE*>(VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(pages);
    memset(pages, 0xcc, 8192);
    assert(VirtualProtect(pages, 8192, PAGE_EXECUTE_READ, &previous));
    ByteChange changes[] = {{pages+2,0xcc,0x18}, {pages+20,0xcc,0x18}, {pages+4098,0xcc,0x11}, {pages+4099,0xcc,0x22}};
    assert(apply_changes(changes, 4));
    for (unsigned i = 0; i < 8192; ++i) {
        const BYTE expected = i == 2 || i == 20 ? 0x18 : i == 4098 ? 0x11 : i == 4099 ? 0x22 : 0xcc;
        assert(pages[i] == expected);
    }
    assert(VirtualQuery(pages, &info, sizeof(info)) && info.Protect == PAGE_EXECUTE_READ);
    assert(VirtualQuery(pages+4096, &info, sizeof(info)) && info.Protect == PAGE_EXECUTE_READ);
    assert(!apply_changes(changes, 4));
    ByteChange mismatch[] = {{pages+50,0xcc,1},{pages+60,0xdd,2}};
    assert(!apply_changes(mismatch, 2));
    assert(pages[50] == 0xcc);
    assert(!apply_changes(changes, 0));
    assert(!apply_changes(changes, 257));
    ByteChange full_patch[107]{};
    for (unsigned i = 0; i < 107; ++i) full_patch[i] = {pages+5000+i,0xcc,static_cast<BYTE>(i)};
    assert(apply_changes(full_patch, 107));
    for (unsigned i = 0; i < 107; ++i) assert(pages[5000+i] == static_cast<BYTE>(i));
    assert(VirtualQuery(pages+4096, &info, sizeof(info)) && info.Protect == PAGE_EXECUTE_READ);
    ByteChange candidate_patch[241]{};
    for (unsigned i = 0; i < 241; ++i) candidate_patch[i] = {pages+6000+i,0xcc,static_cast<BYTE>(i)};
    assert(apply_changes(candidate_patch, 241));
    for (unsigned i = 0; i < 241; ++i) assert(pages[6000+i] == static_cast<BYTE>(i));
    assert(VirtualQuery(pages+4096, &info, sizeof(info)) && info.Protect == PAGE_EXECUTE_READ);
    assert(VirtualFree(pages, 0, MEM_RELEASE));
    FakeWorld world;
    for (Segment s : {Segment{{0,2,0},{0,5,0}}, {{20,2,0},{20,5,0}}, {{2,0,0},{5,0,0}}, {{2,40,0},{5,40,0}}, {{0,5,0},{0,2,0}}})
        assert(boundary_segment(world, s) && boundary_wall_endpoints_valid(world, s));
    for (Segment s : {Segment{{1,2,0},{1,5,0}}, {{0,2,0},{1,5,0}}, {{0,2,0},{0,5,1}}, {{0,-1,0},{0,5,0}}, {{0,2,0},{0,2,0}}, {{0,2,0},{1,2,0}}})
        assert(!boundary_segment(world, s));
    assert(!boundary_wall_endpoints_valid(world, Segment{{0,2.5f,0},{0,5,0}}));
    assert(!boundary_wall_endpoints_valid(world, Segment{{0,2,0},{0,41,0}}));
    assert(!boundary_wall_endpoints_valid(world, Segment{{0,2,0},{0,5,1}}));
    FakeWorld elevated;
    elevated.maximum_level = 5;
    for (int level = 1; level <= 5; ++level) {
        for (Segment s : {Segment{{0,2,level},{0,5,level}}, {{20,2,level},{20,5,level}}, {{2,0,level},{5,0,level}}, {{2,40,level},{5,40,level}}})
            assert(boundary_wall_endpoints_valid(elevated, s));
        Coordinate vertex{0,20,level};
        assert(unlocked_boundary_vertex(elevated, vertex));
        elevated.locked = true;
        assert(!unlocked_boundary_vertex(elevated, vertex));
        elevated.locked = false;
    }
    assert(!boundary_wall_endpoints_valid(elevated, Segment{{0,2,6},{0,5,6}}));
    assert(!boundary_wall_endpoints_valid(elevated, Segment{{0,2,-1},{0,5,-1}}));
    assert(!unlocked_boundary_vertex(elevated, Coordinate{0,20,6}));
    assert(!unlocked_boundary_vertex(elevated, Coordinate{21,20,3}));
    for (SegmentPoint point : {SegmentPoint{0,20,1}, {20,20,1}, {10,0,1}, {10,40,1}, {0,0,1}, {20,40,1}}) {
        FakeWorld roof;
        assert(roof_boundary_corner_valid(roof, point));
        roof.locked = true;
        assert(!roof_boundary_corner_valid(roof, point));
        roof.ignore = true;
        assert(!roof_boundary_corner_valid(roof, point));
    }
    for (SegmentPoint point : {SegmentPoint{10,20,1}, {-1,20,1}, {21,20,1}, {0,41,1}, {0,20,2}, {0,20,-1}, {0,20.5f,1}}) {
        FakeWorld roof;
        assert(!roof_boundary_corner_valid(roof, point));
    }
    puts("PASS: roof boundary sides/corners; existing locks preserved even with ignore-locks option; outside, interior, fractional and invalid-level cases delegate.");
    puts("PASS: upper-level boundary segments and vertices across four sides; existing locks, negative/mixed/unallocated levels and outside coordinates guarded.");
    Segment segment{{0,2,0},{0,5,0}};
    expected_segment = &segment;
    side_adapter = &fake_side_adapter;
    expected_result = false;
    assert(!invoke_side_thunk(reinterpret_cast<void*>(0x1234), &segment));
    expected_result = true;
    assert(invoke_side_thunk(reinterpret_cast<void*>(0x1234), &segment));
    assert(thunk_calls == 2);
    for (Coordinate c : {Coordinate{0,0,0}, {0,20,0}, {20,20,0}, {10,40,0}, {20,40,0}, {10,20,0}}) {
        FakeWorld foundation;
        assert(foundation_corner_valid(foundation, static_cast<float>(c.x), static_cast<float>(c.y)));
        assert(!foundation_corner_locked(foundation, c));
        foundation.locked = true;
        assert(foundation_corner_locked(foundation, c) == (c.x < 20 && c.y < 40));
    }
    for (Coordinate c : {Coordinate{-1,20,0}, {21,20,0}, {10,41,0}, {20,40,1}}) {
        FakeWorld foundation;
        assert(foundation_corner_locked(foundation, c));
    }
    assert(!foundation_corner_valid(world, 0.5f, 20));
    assert(!foundation_corner_valid(world, -1, 20));
    assert(!foundation_corner_valid(world, 21, 20));
    assert(!foundation_corner_valid(world, 10, 41));
    uintptr_t table[0x154/4]{};
    table[0x118/4] = reinterpret_cast<uintptr_t>(&fake_vertex);
    table[0x11c/4] = reinterpret_cast<uintptr_t>(&fake_tile);
    table[0xe4/4] = reinterpret_cast<uintptr_t>(&fake_lock);
    table[0x144/4] = table[0x148/4] = reinterpret_cast<uintptr_t>(&fake_min);
    table[0x14c/4] = reinterpret_cast<uintptr_t>(&fake_max_x);
    table[0x150/4] = reinterpret_cast<uintptr_t>(&fake_max_y);
    WorldFixture fixture{table};
    using FloatCall = bool (__thiscall*)(void*, float, float);
    using CoordCall = bool (__thiscall*)(void*, const Coordinate*);
    const auto float_call = reinterpret_cast<FloatCall>(&foundation_bounds_hook);
    const auto coord_call = reinterpret_cast<CoordCall>(&foundation_lock_hook);
    assert(float_call(&fixture, 5.0f, 6.0f));
    assert(!float_call(&fixture, -1.0f, 6.0f));
    Coordinate corner{5,6,0};
    assert(!coord_call(&fixture, &corner));
    using BoundCall = int32_t (__thiscall*)(void*);
    assert(reinterpret_cast<BoundCall>(&max_vertex_x_hook)(&fixture) == 20);
    assert(reinterpret_cast<BoundCall>(&max_vertex_y_hook)(&fixture) == 40);
    assert(fixture.model.bound(0x14c) == 19 && fixture.model.bound(0x150) == 39);
    assert(fixture.model.vertex_valid(Coordinate{20,40,1}));
    assert(!fixture.model.tile_valid(Coordinate{20,40,1}));
    assert(fixture.model.tile_valid(Coordinate{19,39,1}));
    assert(!fixture.model.vertex_valid(Coordinate{21,40,1}));
    assert(!fixture.model.vertex_valid(Coordinate{20,41,1}));
    fixture.model.locked = true;
    assert(coord_call(&fixture, &corner));
    corner.x = 20;
    assert(!coord_call(&fixture, &corner));
    puts("PASS: foundation corners, absent-tile lock guards, original existing locks, fractional/outside rejection, float and coordinate x86 thiscall-to-fastcall adapters.");
    puts("PASS: boundary base/cap sides/corners, valid tile locks, missing-neighbour guards, geometry scope, x86 EBX/cdecl thunk arguments and boolean returns, exact writes and protection restoration, mismatch rejection.");
}




