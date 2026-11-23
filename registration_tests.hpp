// Included after window_tests.hpp in tests.cpp.
unsigned proposed_registration_direction=4;
bool permitted_registration_shape=true;
void* test_registration_object;
void* __cdecl test_item_object(void*) { return test_registration_object; }
unsigned __cdecl test_direction(void*) { return proposed_registration_direction; }
bool __cdecl test_shape(void*) { return permitted_registration_shape; }
const SegmentPoint* allowed_group_anchor=nullptr;
bool __cdecl test_registration_wall(void*, const SegmentPoint* point, const unsigned* direction, void*) {
    assert(!memcmp(point,actual_window_point,sizeof(*point)) || (allowed_group_anchor && !memcmp(point,allowed_group_anchor,sizeof(*point))));
    assert(*direction==proposed_registration_direction);
    return fixture_wall_attached;
}
__declspec(naked) bool __cdecl invoke_registration_thunk(const Coordinate*,void*,const SegmentPoint*,void*,void* = nullptr,const SegmentPoint* = nullptr) {
    __asm {
        push ebx
        push esi
        sub esp, 60h
        mov esi, [esp+70h]
        mov edx, [esp+74h]
        mov eax, [edx]
        mov [esp+20h], eax
        mov eax, [edx+4]
        mov [esp+24h], eax
        mov eax, [edx+8]
        mov [esp+28h], eax
        mov eax, [esp+80h]
        mov [esp+48h], eax
        mov ebx, [esp+7ch]
        mov ecx, [esp+78h]
        push dword ptr [esp+6ch]
        call registration_tile_thunk
        add esp, 60h
        pop esi
        pop ebx
        ret
    }
}

void* auxiliary_for_flags=nullptr;
unsigned wide_root_profile=0xa1;
unsigned __cdecl wide_flags(void* object){return object==auxiliary_for_flags ? 0 : wide_root_profile;}
bool expect_group_scope=true;
SegmentPoint wide_outside{};
bool __cdecl fake_group_components(void* object,const SegmentPoint* point,const unsigned* direction,float a,float b,unsigned flags,void* errors) {
    assert(object==auxiliary_for_flags && object!=window_fixture && point==allowed_group_anchor && *direction==4);
    assert(a==1.25f && b==-2.5f && flags==0x12345678 && errors==reinterpret_cast<void*>(0x1234));
    assert(window_group.active==expect_group_scope);
    GameWorld world{window_fixture};
    assert(component_support(world,wide_outside,0,4).active==expect_group_scope);
    SetLastError(7654);return expect_group_scope;
}

__declspec(naked) bool __cdecl invoke_group_entry(void*,const SegmentPoint*,const unsigned*,float,float,unsigned,void*,void*) {
    __asm {
        push esi
        mov esi, [esp+24h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        call window_group_entry
        add esp, 1ch
        pop esi
        ret
    }
}

void test_wide_groups(WorldFixture& fixture) {
    WorldFixture auxiliary{fixture.vtable,{}};
    native_flags=wide_flags;auxiliary_for_flags=&auxiliary;
    test_registration_object=&auxiliary;permitted_registration_shape=true;fixture_wall_attached=true;
    SegmentPoint anchor{0.5f,15.5f,0};allowed_group_anchor=&anchor;
    proposed_registration_direction=6;
    GameWorld world{&fixture};
    for (float offset : {0.0f,-1.0f,-2.0f,-3.0f}) {
        SegmentPoint point{-0.5f,15.5f+offset,0};actual_window_point=&point;
        Coordinate tile{-1,static_cast<int32_t>(std::floor(point.y)),0};
        assert(!component_support(world,point,0,6).active);
        assert(invoke_registration_thunk(&tile,&auxiliary,&point,&fixture,&fixture,&anchor));
        assert(!window_group.active && !fixture.model.tile_valid(tile));
    }
    SegmentPoint too_far{-0.5f,11.5f,0};actual_window_point=&too_far;
    Coordinate bad{-1,11,0};
    assert(!invoke_registration_thunk(&bad,&auxiliary,&too_far,&fixture,&fixture,&anchor));
    // Exercise the seven-argument group call with original pointers and values.
    anchor={5.5f,39.5f,0};wide_outside={3.5f,40.5f,0};actual_window_point=&wide_outside;
    unsigned direction=4;proposed_registration_direction=4;
    native_group_components=fake_group_components;
    for (unsigned profile : {0xa1u,0xa2u,0xa4u,0xa8u,0x8a1u,0x8a2u,0x8a4u,0x8a8u,0xc0u}) {
        wide_root_profile=profile;
        assert(invoke_group_entry(&auxiliary,&anchor,&direction,1.25f,-2.5f,0x12345678,reinterpret_cast<void*>(0x1234),&fixture));
        assert(GetLastError()==7654 && !window_group.active && direction==4);
    }
    assert(!window_component_flags(0xc0) && window_group_flags(0xc0));
    fixture_wall_flags=0;native_flags=fixture_flags;expect_group_scope=false;
    assert(!invoke_group_entry(&auxiliary,&anchor,&direction,1.25f,-2.5f,0x12345678,reinterpret_cast<void*>(0x1234),&fixture));
    assert(GetLastError()==7654 && !window_group.active);
    allowed_group_anchor=nullptr;
    puts("PASS: grouped zero-flag auxiliary components at width offsets 0/1/2; unrelated/farther components rejected; owner/anchor x86 stack capture; seven-argument group ABI, direction, LastError and scope restoration.");
}

bool fake_occupies_value;
bool __fastcall fake_occupies(void*,void*) { return fake_occupies_value; }
void test_registration() {
    uintptr_t table[0x300/4]{};
    table[0x11c/4]=reinterpret_cast<uintptr_t>(&fake_tile);
    table[0xe4/4]=reinterpret_cast<uintptr_t>(&fake_lock);
    table[0x144/4]=table[0x148/4]=reinterpret_cast<uintptr_t>(&fake_min);
    table[0x14c/4]=reinterpret_cast<uintptr_t>(&fake_max_x);
    table[0x150/4]=reinterpret_cast<uintptr_t>(&fake_max_y);
    table[0x2f0/4]=reinterpret_cast<uintptr_t>(&fake_occupies);
    WorldFixture fixture{table,{}};
    window_fixture=&fixture;
    test_registration_object=&fixture;
    registration_object=test_item_object;
    registration_direction=test_direction;
    registration_shape=test_shape;
    native_flags=fixture_flags; fixture_wall_flags=0xa1;
    native_wall_check=test_registration_wall; fixture_wall_attached=true;
    for (SegmentPoint point : {SegmentPoint{-0.5f,5.5f,0},{20.5f,5.5f,0},{5.5f,-0.5f,0},{5.5f,40.5f,0}}) {
        for (unsigned d : {0u,2u,4u,6u}) {
            proposed_registration_direction=d; actual_window_point=&point;
            Coordinate tile{static_cast<int32_t>(std::floor(point.x)),static_cast<int32_t>(std::floor(point.y)),point.level,9};
            const SegmentPoint before=point; const Coordinate before_tile=tile;
            assert(invoke_registration_thunk(&tile,&fixture,&point,&fixture));
            assert(!memcmp(&point,&before,sizeof(point)) && !memcmp(&tile,&before_tile,sizeof(tile)));
            // The branch adapter admits assignment, but native world validity
            // remains false. No virtual tile is exposed to other consumers.
            assert(!fixture.model.tile_valid(tile));
            assert(!window_support.active && proposed_registration_direction==d);
        }
    }
    SegmentPoint point{5.5f,40.5f,0};actual_window_point=&point; proposed_registration_direction=4;
    Coordinate tile{5,40,0};
    permitted_registration_shape=false;
    assert(!invoke_registration_thunk(&tile,&fixture,&point,&fixture));
    permitted_registration_shape=true;fixture_wall_attached=false;
    assert(!invoke_registration_thunk(&tile,&fixture,&point,&fixture));
    fixture_wall_attached=true;fixture.model.locked=true;
    assert(!invoke_registration_thunk(&tile,&fixture,&point,&fixture));
    fixture.model.locked=false;fixture_wall_flags=0;
    assert(!invoke_registration_thunk(&tile,&fixture,&point,&fixture));
    fixture_wall_flags=0xa1;test_registration_object=nullptr;
    assert(!invoke_registration_thunk(&tile,&fixture,&point,&fixture));
    test_registration_object=&fixture;proposed_registration_direction=1;
    assert(!invoke_registration_thunk(&tile,&fixture,&point,&fixture));
    proposed_registration_direction=4;tile.x=6;
    assert(!invoke_registration_thunk(&tile,&fixture,&point,&fixture));
    tile={5,39,0};test_registration_object=nullptr;
    assert(invoke_registration_thunk(&tile,nullptr,&point,&fixture));
    // Exercise the actual unsafe-path selector with a readable fake object.
    alignas(uintptr_t) unsigned char object[512]{};
    *reinterpret_cast<uintptr_t**>(object)=table;
    for (bool occupies : {false,true}) for (unsigned char alternate : {static_cast<unsigned char>(0),static_cast<unsigned char>(0x20)}) {
        fake_occupies_value=occupies;object[0xef]=alternate;
        assert(supported_registration_shape(object)==(!occupies || !alternate));
    }
    test_wide_groups(fixture);
    puts("PASS: assignment branch x86 stack-local/ESI thunk; four sides/four rotations; no position/direction writes or global tile-valid override; unsafe occupancy, locks, missing objects, bad directions and mismatched tiles rejected.");
}

