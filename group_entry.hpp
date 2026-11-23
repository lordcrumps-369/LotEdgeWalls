// At the call from 0xc32380 to 0xc32130, arg1 is the queried group
// interface, while ESI is still the actual object passed to 0xc32380.
// Preserve the interface and every original argument; supply ESI separately.
bool __cdecl window_group_from_root(void* interface_pointer,const SegmentPoint* point,const unsigned* direction,
    float a,float b,unsigned flags,void* errors,void* root) {
    const auto context=direction ? validated_window_group(root,point,*direction) : WindowGroup{};
    WindowGroupScope scope(window_group,context);
    const bool result=native_group_components(interface_pointer,point,direction,a,b,flags,errors);
    if (root && point && direction) {

    }
    return result;
}
__declspec(naked) bool __cdecl window_group_entry(void*,const SegmentPoint*,const unsigned*,float,float,unsigned,void*) {
    __asm {
        push esi
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        push dword ptr [esp+20h]
        call window_group_from_root
        add esp, 20h
        ret
    }
}
