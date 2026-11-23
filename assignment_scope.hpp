// Scope the actual native virtual setter call, with its real object, requested
// point, and two original 32-bit argument words. No inference from later
// component-loop locals is needed for this path.
using NativeSetGroup = void (__thiscall*)(void*,const SegmentPoint*,uint32_t,uint32_t);
NativeSetGroup assignment_override=nullptr; // Synthetic tests only; null in ASI.
void __fastcall assign_window_group(void* self,void*,const SegmentPoint* point,uint32_t word2,uint32_t word3) {
    // These setter words are not declared to be rotation. Read the native
    // object's actual direction and preserve the original words bit for bit.
    const unsigned direction=registration_direction(self);
    const auto candidate=validated_window_group(self,point,direction);
    // Auxiliary component setters may nest without their own eligible root.
    // Keep the active parent context in that case, restoring it on return.
    WindowGroupScope scope(window_group,candidate.active ? candidate : window_group);

    const auto native=assignment_override ? assignment_override : method<NativeSetGroup>(self,0x4f4);
    native(self,point,word2,word3);
}
