#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include "boundary_policy.hpp"
#include "window_support.hpp"
#include "window_group.hpp"

#include "compatible_image.hpp"

namespace {
std::atomic<bool> initialized{false};
uintptr_t game_base;
template<class Function>
Function game_function(uintptr_t va) { return reinterpret_cast<Function>(game_base + va - 0x400000); }
template<class Function>
Function method(void* self, unsigned offset) {
    return reinterpret_cast<Function>((*static_cast<uintptr_t**>(self))[offset / 4]);
}
struct GameWorld {
    void* self;
    bool vertex_valid(const Coordinate& c) { return method<bool (__thiscall*)(void*, const Coordinate*)>(self, 0x118)(self, &c); }
    bool tile_valid(const Coordinate& c) { return method<bool (__thiscall*)(void*, const Coordinate*)>(self, 0x11c)(self, &c); }
    bool tile_locked(const Coordinate& c) { return method<bool (__thiscall*)(void*, const Coordinate*)>(self, 0xe4)(self, &c); }
    int32_t bound(unsigned offset) { return method<int32_t (__thiscall*)(void*)>(self, offset)(self); }
    bool ignore_locks() {
        void* options = game_function<void* (__cdecl*)()>(0x799a38)();
        return method<bool (__thiscall*)(void*)>(options, 0xdc)(options);
    }
};


constexpr BYTE strict_call[] = {0xff, 0x90, 0x24, 0x01, 0x00, 0x00};
bool matches(const void* address, const void* expected, size_t size) {
    BYTE bytes[64]{};
    SIZE_T read = 0;
    return size <= sizeof(bytes) && ReadProcessMemory(GetCurrentProcess(), address, bytes, size, &read) &&
           read == size && !memcmp(bytes, expected, size);
}
bool patch_calls(BYTE* first, BYTE* second) {
    if (second <= first || second - first > 64 ||
        !matches(first, strict_call, sizeof(strict_call)) || !matches(second, strict_call, sizeof(strict_call))) return false;
    const SIZE_T span = static_cast<SIZE_T>(second - first) + sizeof(strict_call);
    DWORD protection = 0;
    if (!VirtualProtect(first, span, PAGE_EXECUTE_READWRITE, &protection)) return false;
    // Only these two endpoint validations change: strict tile interior (+0x124)
    // becomes the existing inclusive vertex check (+0x118), with the same ABI.
    *reinterpret_cast<volatile BYTE*>(first + 2) = 0x18;
    *reinterpret_cast<volatile BYTE*>(second + 2) = 0x18;
    FlushInstructionCache(GetCurrentProcess(), first, span);
    DWORD ignored = 0;
    VirtualProtect(first, span, protection, &ignored);
    return true;
}
struct ByteChange { BYTE* address; BYTE expected; BYTE replacement; };
bool apply_changes(const ByteChange* changes, unsigned size) {
    if (!size || size > 256) return false;
    DWORD protections[256]{};
    for (unsigned i = 0; i < size; ++i)
        if (!matches(changes[i].address, &changes[i].expected, 1)) return false;
    for (unsigned i = 0; i < size; ++i) {
        if (!VirtualProtect(changes[i].address, 1, PAGE_EXECUTE_READWRITE, &protections[i])) {
            while (i) { --i; DWORD ignored = 0; VirtualProtect(changes[i].address, 1, protections[i], &ignored); }
            return false;
        }
    }
    for (unsigned i = 0; i < size; ++i) *reinterpret_cast<volatile BYTE*>(changes[i].address) = changes[i].replacement;
    for (unsigned i = size; i; --i) {
        DWORD ignored = 0;
        FlushInstructionCache(GetCurrentProcess(), changes[i-1].address, 1);
        VirtualProtect(changes[i-1].address, 1, protections[i-1], &ignored);
    }
    return true;
}
bool __cdecl wall_vertex_locked(const Coordinate* vertex, void* errors) {
    GameWorld world{game_function<void* (__cdecl*)()>(0x4ae0dc)()};
    if (vertex && world.self && unlocked_boundary_vertex(world, *vertex)) {
        return false;
    }
    return game_function<bool (__cdecl*)(const Coordinate*, void*)>(0x6020ed)(vertex, errors);
}
bool __cdecl boundary_single_side(const void* partition, const Segment* segment) {
    const bool original = game_function<bool (__cdecl*)(const void*)>(0xc28010)(partition);
    if (original || !segment) return original;
    GameWorld world{game_function<void* (__cdecl*)()>(0x4ae0dc)()};
    if (!world.self || !boundary_wall_endpoints_valid(world, *segment)) return false;
    return true;
}
bool __cdecl boundary_wall_bounds(const Segment* segment, const void* partition, void* errors) {
    GameWorld world{game_function<void* (__cdecl*)()>(0x4ae0dc)()};
    if (segment && world.self && boundary_wall_endpoints_valid(world, *segment)) {
        return true;
    }
    return game_function<bool (__cdecl*)(const Segment*, const void*, void*)>(0x5fe394)(segment, partition, errors);
}
bool __fastcall foundation_bounds_hook(void* self, void*, float x, float y) {
    GameWorld world{self};
    const bool valid = foundation_corner_valid(world, x, y);

    return valid;
}
bool __fastcall foundation_lock_hook(void* self, void*, const Coordinate* corner) {
    if (!corner) return true;
    GameWorld world{self};
    return foundation_corner_locked(world, *corner);
}
int32_t __fastcall max_vertex_x_hook(void* self, void*) {
    GameWorld world{self};
    const int32_t maximum = world.bound(0x14c);
    return maximum == std::numeric_limits<int32_t>::max() ? maximum : maximum + 1;
}
int32_t __fastcall max_vertex_y_hook(void* self, void*) {
    GameWorld world{self};
    const int32_t maximum = world.bound(0x150);
    return maximum == std::numeric_limits<int32_t>::max() ? maximum : maximum + 1;
}
bool __cdecl roof_corner_hook(const SegmentPoint* point) {
    GameWorld world{game_function<void* (__cdecl*)()>(0x4ae0dc)()};
    if (point && world.self && roof_boundary_corner_valid(world, *point)) {
        return true;
    }
    return game_function<bool (__cdecl*)(const SegmentPoint*)>(0x60c927)(point);
}
#include "window_hooks.hpp"
#include "registration_hooks.hpp"
#include "assignment_scope.hpp"
using SideAdapter = bool (__cdecl*)(const void*, const Segment*);
SideAdapter side_adapter = &boundary_single_side;
// At this verified call site EBX holds the wall segment. Keep the original
// one-argument cdecl stack layout while supplying it to the scoped adapter.
__declspec(naked) bool __cdecl single_side_thunk(const void*) {
    __asm {
        push ebx
        push dword ptr [esp+8]
        call dword ptr [side_adapter]
        add esp, 8
        ret
    }
}
}

extern "C" __declspec(dllexport) void InitializeASI() {
    if (initialized.exchange(true)) return;
    const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    game_base = base;
    if (!compatibility::accepts(base)) return;
    const auto address = [base](uintptr_t va) { return reinterpret_cast<BYTE*>(base + va - 0x400000); };
    const uintptr_t vertex_target = base + 0x10231b0 - 0x400000;
    const uintptr_t strict_target = base + 0x1023330 - 0x400000;
    if (!matches(address(0x12d9988), &vertex_target, sizeof(vertex_target)) ||
        !matches(address(0x12d9994), &strict_target, sizeof(strict_target))) {
         return;
    }
    constexpr BYTE first_context[] = {0xff,0x90,0x24,0x01,0x00,0x00,0x84,0xc0,0x74,0x12};
    constexpr BYTE second_context[] = {0xff,0x90,0x24,0x01,0x00,0x00,0x84,0xc0,0x75,0x2f};
    if (!matches(address(0x5fe3f1), first_context, sizeof(first_context)) ||
        !matches(address(0x5fe403), second_context, sizeof(second_context))) {
         return;
    }
    constexpr BYTE locked_call[] = {0xe8,0xfd,0xed,0xff,0xff,0x84,0xc0,0x59,0x59};
    if (!matches(address(0x6032eb), locked_call, sizeof(locked_call))) {
         return;
    }
    constexpr BYTE side_call[] = {0xe8,0x3c,0x7f,0x62,0x00,0x83,0xc4,0x14};
    if (!matches(address(0x6000cf), side_call, sizeof(side_call))) {
         return;
    }
    constexpr BYTE bounds_call_a[] = {0xe8,0x1e,0xdc,0xff,0xff,0x83,0xc4,0x0c};
    constexpr BYTE bounds_call_b[] = {0xe8,0xcc,0xdb,0xff,0xff};
    if (!matches(address(0x600771), bounds_call_a, sizeof(bounds_call_a)) ||
        !matches(address(0x6007c3), bounds_call_b, sizeof(bounds_call_b))) {
         return;
    }
    constexpr BYTE foundation_bounds_call[] = {0xff,0x90,0x28,0x01,0x00,0x00};
    constexpr BYTE foundation_lock_call[] = {0xff,0x90,0xe4,0x00,0x00,0x00};
    const uintptr_t foundation_sites[] = {0x4d9a07,0x4d9a7c,0x4d9a1f,0x4d9a91};
    for (unsigned i = 0; i < 4; ++i) {
        if (!matches(address(foundation_sites[i]), i < 2 ? foundation_bounds_call : foundation_lock_call, 6)) {
             return;
        }
    }
    constexpr BYTE max_x_call[] = {0xff,0x90,0x4c,0x01,0x00,0x00};
    constexpr BYTE max_y_call[] = {0xff,0x90,0x50,0x01,0x00,0x00};
    constexpr BYTE min_x_branch[] = {0x0f,0x8e,0x04,0x01,0x00,0x00};
    constexpr BYTE min_y_branch[] = {0x0f,0x8e,0xdf,0x00,0x00,0x00};
    if (!matches(address(0x60a9ad), max_x_call, 6) || !matches(address(0x60a9d6), max_y_call, 6) ||
        !matches(address(0x60a9a3), min_x_branch, 6) || !matches(address(0x60a9c8), min_y_branch, 6) ||
        !matches(address(0x603736), strict_call, 6) || !matches(address(0xc239fe), strict_call, 6)) {
         return;
    }
    const uintptr_t roof_sites[] = {0x4cc60a,0x4cc613,0x4cc764,0x4cc772,0x4cc780,0x4cc78e};
    for (uintptr_t site : roof_sites) {
        BYTE expected[5] = {0xe8,0,0,0,0};
        const uint32_t relative_original = static_cast<uint32_t>(0x60c927 - (site+5));
        memcpy(expected+1, &relative_original, sizeof(relative_original));
        if (!matches(address(site), expected, sizeof(expected))) {
             return;
        }
    }
    const uintptr_t removal_sites[] = {0x4d8b6a,0x4d8bd4,0x4d8b7f,0x4d8be9};
    for (unsigned i = 0; i < 4; ++i) {
        if (!matches(address(removal_sites[i]), i < 2 ? foundation_bounds_call : foundation_lock_call, 6)) {
             return;
        }
    }
    if (!matches(address(0x603be3), strict_call, 6)) {
         return;
    }

    struct WindowCall { uintptr_t va, function; unsigned length; BYTE original[6]; };
    const WindowCall window_calls[] = {
    {0x8c500b, reinterpret_cast<uintptr_t>(&assign_window_group), 6, {0xff,0x90,0xf4,0x04,0x00,0x00}},
    {0xc323e3, reinterpret_cast<uintptr_t>(&window_group_entry), 5, {0xe8,0x48,0xfd,0xff,0xff}},
    {0x8c71d4, reinterpret_cast<uintptr_t>(&registration_tile_thunk), 6, {0xff,0x92,0x1c,0x01,0x00,0x00}},
    {0xc31a67, reinterpret_cast<uintptr_t>(&window_common), 5, {0xe8,0x94,0xef,0xff,0xff}},
    {0xc30abf, reinterpret_cast<uintptr_t>(&support_xy), 6, {0xff,0x90,0x20,0x01,0x00,0x00}},
    {0xc30be4, reinterpret_cast<uintptr_t>(&support_xy), 6, {0xff,0x92,0x20,0x01,0x00,0x00}},
    {0xc30c14, reinterpret_cast<uintptr_t>(&support_xy), 6, {0xff,0x92,0x20,0x01,0x00,0x00}},
    {0xc30c45, reinterpret_cast<uintptr_t>(&support_xy), 6, {0xff,0x92,0x20,0x01,0x00,0x00}},
    {0xc30c70, reinterpret_cast<uintptr_t>(&support_xy), 6, {0xff,0x92,0x20,0x01,0x00,0x00}},
    {0xc30b90, reinterpret_cast<uintptr_t>(&support_tile_bounds), 6, {0xff,0x90,0x1c,0x01,0x00,0x00}},
    {0xc30ba5, reinterpret_cast<uintptr_t>(&support_tile_bounds), 6, {0xff,0x92,0x1c,0x01,0x00,0x00}},
    {0xc30a3e, reinterpret_cast<uintptr_t>(&support_tile_conversion), 5, {0xe8,0x5d,0x65,0xff,0xff}},
    {0xc3114b, reinterpret_cast<uintptr_t>(&support_tile_conversion), 5, {0xe8,0x50,0x5e,0xff,0xff}},
    {0xc3117c, reinterpret_cast<uintptr_t>(&support_tile_conversion), 5, {0xe8,0x1f,0x5e,0xff,0xff}},
    {0xc311ab, reinterpret_cast<uintptr_t>(&support_tile_conversion), 5, {0xe8,0xf0,0x5d,0xff,0xff}},
    {0xc311e3, reinterpret_cast<uintptr_t>(&support_tile_conversion), 5, {0xe8,0xb8,0x5d,0xff,0xff}},
    {0xc30ca0, reinterpret_cast<uintptr_t>(&support_mask_conversion), 5, {0xe8,0x8b,0x63,0xff,0xff}},
    {0xc30d80, reinterpret_cast<uintptr_t>(&support_mask_conversion), 5, {0xe8,0xab,0x62,0xff,0xff}},
    {0xc3181c, reinterpret_cast<uintptr_t>(&support_mask_conversion), 5, {0xe8,0x0f,0x58,0xff,0xff}},
    {0xc30db5, reinterpret_cast<uintptr_t>(&support_point_check), 5, {0xe8,0xb6,0x3b,0xff,0xff}},
    {0xc3126c, reinterpret_cast<uintptr_t>(&support_point_flags), 5, {0xe8,0xaf,0x34,0xff,0xff}},
    {0xc31498, reinterpret_cast<uintptr_t>(&support_point_flags), 5, {0xe8,0x83,0x32,0xff,0xff}},
    {0xc314f6, reinterpret_cast<uintptr_t>(&support_point_flags), 5, {0xe8,0x25,0x32,0xff,0xff}},
    {0xc3170f, reinterpret_cast<uintptr_t>(&support_object_surface), 5, {0xe8,0x8c,0x76,0xff,0xff}},
    {0xc3175d, reinterpret_cast<uintptr_t>(&support_object_room), 5, {0xe8,0x4e,0x7b,0xff,0xff}},
    };
    for (const auto& call : window_calls) {
        if (!matches(address(call.va), call.original, call.length)) {
             return;
        }
    }
    native_group_components = game_function<GroupComponents>(0xc32130);
    native_common = game_function<PlacementCommon>(0xc30a00);
    native_wall_check = game_function<PlacementCommon>(0xc2af00);
    native_world = game_function<WorldAccess>(0x4ae0dc);
    native_tile_conversion = game_function<ConvertPoint>(0xc26fa0);
    native_mask_conversion = game_function<ConvertPoint>(0xc27030);
    const uint32_t relative = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&wall_vertex_locked) - reinterpret_cast<uintptr_t>(address(0x6032f0)));
    BYTE displacement[4]{};
    memcpy(displacement, &relative, sizeof(relative));
    ByteChange changes[256] = {{address(0x5fe3f3),0x24,0x18}, {address(0x5fe405),0x24,0x18}};
    for (unsigned i = 0; i < 4; ++i) changes[i+2] = {address(0x6032ec+i), locked_call[i+1], displacement[i]};
    const uint32_t side_relative = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&single_side_thunk) - reinterpret_cast<uintptr_t>(address(0x6000d4)));
    BYTE side_displacement[4]{};
    memcpy(side_displacement, &side_relative, sizeof(side_relative));
    for (unsigned i = 0; i < 4; ++i) changes[i+6] = {address(0x6000d0+i), side_call[i+1], side_displacement[i]};
    const uintptr_t bounds_sites[] = {0x600771, 0x6007c3};
    const BYTE* bounds_originals[] = {bounds_call_a, bounds_call_b};
    for (unsigned site = 0; site < 2; ++site) {
        const uint32_t target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&boundary_wall_bounds) - reinterpret_cast<uintptr_t>(address(bounds_sites[site]+5)));
        BYTE bytes[4]{};
        memcpy(bytes, &target, sizeof(target));
        for (unsigned i = 0; i < 4; ++i) changes[10+site*4+i] = {address(bounds_sites[site]+1+i),bounds_originals[site][i+1],bytes[i]};
    }
    for (unsigned site = 0; site < 4; ++site) {
        const uintptr_t target = site < 2 ? reinterpret_cast<uintptr_t>(&foundation_bounds_hook) : reinterpret_cast<uintptr_t>(&foundation_lock_hook);
        const uint32_t relative_call = static_cast<uint32_t>(target - reinterpret_cast<uintptr_t>(address(foundation_sites[site]+5)));
        BYTE replacement[6] = {0xe8,0,0,0,0,0x90};
        memcpy(replacement+1, &relative_call, sizeof(relative_call));
        const BYTE* expected = site < 2 ? foundation_bounds_call : foundation_lock_call;
        for (unsigned i = 0; i < 6; ++i) changes[18+site*6+i] = {address(foundation_sites[site]+i),expected[i],replacement[i]};
    }
    const uintptr_t height_max_sites[] = {0x60a9ad,0x60a9d6};
    const uintptr_t height_max_targets[] = {reinterpret_cast<uintptr_t>(&max_vertex_x_hook),reinterpret_cast<uintptr_t>(&max_vertex_y_hook)};
    const BYTE* height_max_originals[] = {max_x_call,max_y_call};
    for (unsigned site = 0; site < 2; ++site) {
        const uint32_t relative_call = static_cast<uint32_t>(height_max_targets[site] - reinterpret_cast<uintptr_t>(address(height_max_sites[site]+5)));
        BYTE replacement[6] = {0xe8,0,0,0,0,0x90};
        memcpy(replacement+1, &relative_call, sizeof(relative_call));
        for (unsigned i = 0; i < 6; ++i) changes[42+site*6+i] = {address(height_max_sites[site]+i),height_max_originals[site][i],replacement[i]};
    }
    // The routine already checks physical vertex validity before these margins.
    changes[54] = {address(0x60a9a4),0x8e,0x8c};
    changes[55] = {address(0x60a9c9),0x8e,0x8c};
    // These are TILE checks: use inclusive tile bounds, never vertex bounds.
    changes[56] = {address(0x603738),0x24,0x1c};
    changes[57] = {address(0xc23a00),0x24,0x1c};
    for (unsigned site = 0; site < 6; ++site) {
        const uint32_t original = static_cast<uint32_t>(0x60c927 - (roof_sites[site]+5));
        const uint32_t replacement = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&roof_corner_hook) - reinterpret_cast<uintptr_t>(address(roof_sites[site]+5)));
        BYTE expected_bytes[4]{}, new_bytes[4]{};
        memcpy(expected_bytes, &original, 4);
        memcpy(new_bytes, &replacement, 4);
        for (unsigned i = 0; i < 4; ++i) changes[58+site*4+i] = {address(roof_sites[site]+1+i),expected_bytes[i],new_bytes[i]};
    }
    for (unsigned site = 0; site < 4; ++site) {
        const uintptr_t target = site < 2 ? reinterpret_cast<uintptr_t>(&foundation_bounds_hook) : reinterpret_cast<uintptr_t>(&foundation_lock_hook);
        const uint32_t relative_call = static_cast<uint32_t>(target - reinterpret_cast<uintptr_t>(address(removal_sites[site]+5)));
        BYTE replacement[6] = {0xe8,0,0,0,0,0x90};
        memcpy(replacement+1, &relative_call, sizeof(relative_call));
        const BYTE* expected = site < 2 ? foundation_bounds_call : foundation_lock_call;
        for (unsigned i = 0; i < 6; ++i) changes[82+site*6+i] = {address(removal_sites[site]+i),expected[i],replacement[i]};
    }
    changes[106] = {address(0x603be5),0x24,0x1c};
    unsigned change_count = 107;
    for (const auto& call : window_calls) {
        const uint32_t relative_call = static_cast<uint32_t>(call.function - reinterpret_cast<uintptr_t>(address(call.va+5)));
        BYTE replacement[6] = {0xe8,0,0,0,0,0x90};
        memcpy(replacement+1, &relative_call, 4);
        for (unsigned i=0; i<call.length; ++i)
            changes[change_count++] = {address(call.va+i),call.original[i],replacement[i]};
    }
    if (!apply_changes(changes, change_count)) {  return; }
    
    
    
    
    
    
    
    
    
    
    
    
    
}
static DWORD WINAPI initialize_from_loader(void*) { InitializeASI(); return 0; }
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread=CreateThread(nullptr,0,initialize_from_loader,nullptr,0,nullptr);
        if(thread)CloseHandle(thread);
    }
    return TRUE;
}
