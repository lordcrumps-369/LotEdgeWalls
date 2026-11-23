// Included inside plugin.cpp's anonymous namespace. This is a single branch
// adapter, not a replacement WorldDB predicate and not a world-array remap.
using ItemObject = void* (__cdecl*)(void*);
using ObjectDirection = unsigned (__cdecl*)(void*);
using RegistrationShape = bool (__cdecl*)(void*);
void* __cdecl component_object(void* item) {
    return method<void* (__thiscall*)(void*)>(item, 0x2c)(item);
}
unsigned __cdecl object_direction(void* object) {
    return method<unsigned (__thiscall*)(void*)>(object, 0xac)(object);
}
bool __cdecl supported_registration_shape(void* object) {
    // The alternative registration path calls WorldDB+0x68 / 0x10257f0,
    // which reaches an unchecked object-list index in 0x1025110. Do not admit
    // objects selecting that path. Normal single/footprint insertion paths
    // have their own native tile guards; leave those guards untouched.
    const bool occupies = method<bool (__thiscall*)(void*)>(object, 0x2f0)(object);
    const bool alternate = (static_cast<const unsigned char*>(object)[0xef] & 0x20) != 0;
    return !occupies || !alternate;
}
ItemObject registration_object = component_object;
ObjectDirection registration_direction = object_direction;
RegistrationShape registration_shape = supported_registration_shape;

bool __cdecl registration_tile_allowed(void* self, const Coordinate* tile, void* item, const SegmentPoint* point, unsigned direction) {
    GameWorld world{self};
    if (!self || !tile) return false;
    if (world.tile_valid(*tile)) return true;
    if (!item || !point || !std::isfinite(point->x) || !std::isfinite(point->y) ||
        std::fabs(point->x) > 1000000 || std::fabs(point->y) > 1000000 ||
        tile->z != point->level || tile->x != static_cast<int32_t>(std::floor(point->x)) ||
        tile->y != static_cast<int32_t>(std::floor(point->y))) {  return false; }
    void* object = registration_object(item);
    if (!object) {  return false; }
    const unsigned component_flags=native_flags(object);
    const auto support = component_support(world, *point, component_flags, direction);
    if (!support.active) {  return false; }
    // Use the actual object direction via +0xac. Do not infer rotation from
    // another argument of the position setter or change its calling convention.
    const bool supported = registration_shape(object);
    const bool attached = supported && native_wall_check(object, point, &direction, nullptr);

    return attached;
}

// At 0x8c71d4: ECX=WorldDB, ESI=component item, [ESP+4]=tile.
// The original point local at caller ESP+0x20 lies at thunk ESP+0x28.
// Capture it without changing the caller's locals or nonvolatile registers.
bool __cdecl registration_entry(void* self, const Coordinate* tile, void* item, const SegmentPoint* point, void* owner, const SegmentPoint* anchor) {
    if (self && tile && GameWorld{self}.tile_valid(*tile)) return true;
    void* object = item ? registration_object(item) : nullptr;
    const unsigned direction = window_group.active ? window_group.direction : (object ? registration_direction(object) : 0xffffffff);
    if (window_group.active) return registration_tile_allowed(self,tile,item,point,direction);
    const auto group=validated_window_group(owner,anchor,direction);
    WindowGroupScope scope(window_group,group);
    return registration_tile_allowed(self, tile, item, point, direction);
}
__declspec(naked) bool __fastcall registration_tile_thunk(void*, void*, const Coordinate*) {
    __asm {
        push dword ptr [esp+50h]
        push ebx
        lea eax, [esp+30h]
        push eax
        push esi
        push dword ptr [esp+14h]
        push ecx
        call registration_entry
        add esp, 18h
        ret 4
    }
}
