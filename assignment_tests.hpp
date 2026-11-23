bool expected_assignment_scope=true;
unsigned assignment_calls=0;
WorldFixture* assignment_auxiliary;
const SegmentPoint* assignment_anchor;
SegmentPoint assignment_outside{};
void __fastcall fake_group_setter(void* self,void*,const SegmentPoint* point,uint32_t word2,uint32_t word3) {
    ++assignment_calls;
    assert(window_group.active==expected_assignment_scope);
    if (self==window_fixture) {
        assert(point==assignment_anchor && word2==0x3fa00000 && word3==0xdeadbeef);
        if (expected_assignment_scope)
            assign_window_group(assignment_auxiliary,nullptr,&assignment_outside,0x40000000,0xfedcba98);
    } else {
        assert(self==assignment_auxiliary && point==&assignment_outside && word2==0x40000000 && word3==0xfedcba98);
        Coordinate tile{-1,4,0};
        assert(registration_entry(window_fixture,&tile,assignment_auxiliary,&assignment_outside,nullptr,nullptr));
        assert(!window_fixture->model.tile_valid(tile));
    }
    SetLastError(7654);
}
void test_assignment_scope() {
    uintptr_t table[0x168/4]{};
    table[0x11c/4]=reinterpret_cast<uintptr_t>(&fake_tile);
    table[0xe4/4]=reinterpret_cast<uintptr_t>(&fake_lock);
    table[0x144/4]=table[0x148/4]=reinterpret_cast<uintptr_t>(&fake_min);
    table[0x14c/4]=reinterpret_cast<uintptr_t>(&fake_max_x);
    table[0x150/4]=reinterpret_cast<uintptr_t>(&fake_max_y);
    WorldFixture root{table,{}},auxiliary{table,{}};
    window_fixture=&root;assignment_auxiliary=&auxiliary;
    SegmentPoint anchor{0.5f,5.5f,0};assignment_anchor=&anchor;
    assignment_outside={-0.5f,4.5f,0};actual_window_point=&assignment_outside;allowed_group_anchor=&anchor;
    native_world=fixture_world;native_flags=wide_flags;wide_root_profile=0xa4;auxiliary_for_flags=&auxiliary;
    native_wall_check=test_registration_wall;fixture_wall_attached=true;
    registration_object=test_item_object;test_registration_object=&auxiliary;
    registration_direction=test_direction;proposed_registration_direction=6;
    registration_shape=test_shape;permitted_registration_shape=true;
    assignment_override=reinterpret_cast<NativeSetGroup>(&fake_group_setter);
    const SegmentPoint saved=anchor;
    reinterpret_cast<NativeSetGroup>(&assign_window_group)(&root,&anchor,0x3fa00000,0xdeadbeef);
    assert(assignment_calls==2 && GetLastError()==7654);
    assert(!window_group.active && !memcmp(&anchor,&saved,sizeof(anchor)) && proposed_registration_direction==6);
    expected_assignment_scope=false;wide_root_profile=0;
    reinterpret_cast<NativeSetGroup>(&assign_window_group)(&root,&anchor,0x3fa00000,0xdeadbeef);
    assert(assignment_calls==3 && !window_group.active && GetLastError()==7654);
    // The observed 0x08a1 auxiliary is admitted without requiring a parent
    // scope; its actual original flag value still reaches native wall checks.
    native_flags=fixture_flags;fixture_wall_flags=0x8a1;
    Coordinate outside_tile{-1,4,0};test_registration_object=&auxiliary;
    assert(!window_group.active);
    assert(registration_entry(&root,&outside_tile,&auxiliary,&assignment_outside,nullptr,nullptr));
    assert(fixture_wall_flags==0x8a1 && proposed_registration_direction==6);
    assert(!root.model.tile_valid(outside_tile));
    fixture_wall_flags=0x18a1;
    assert(!registration_entry(&root,&outside_tile,&auxiliary,&assignment_outside,nullptr,nullptr));
    assignment_override=nullptr;allowed_group_anchor=nullptr;
    puts("PASS: real setter scope inherits across auxiliary nesting; assignment admits grouped zero-flag child without rebuilding owner context; raw float/extra argument words preserved; direction read separately; native delegation and scope restore.");
}
