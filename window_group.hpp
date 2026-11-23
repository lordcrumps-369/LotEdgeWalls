struct WindowGroup {
    bool active=false;
    SegmentPoint anchor{};
    unsigned direction=0xffffffffu;
    bool covers(const WindowSupport& support, const SegmentPoint& outside) const {
        if (!active || !support.active || direction>6 || (direction&1) || anchor.level!=outside.level) return false;
        SegmentPoint inside{};
        if (!support.map_point(outside,inside)) return false;
        const float a=support.axis==0 ? anchor.x : anchor.y;
        const float s=support.axis==0 ? inside.x : inside.y;
        const float tangent=support.axis==0 ? anchor.y : anchor.x;
        const float other=support.axis==0 ? outside.y : outside.x;
        return a==s && std::isfinite(tangent) && std::fabs(tangent-other)<=3;
    }
};
struct WindowGroupScope {
    WindowGroup& slot; WindowGroup previous;
    WindowGroupScope(WindowGroup& s,WindowGroup value):slot(s),previous(s){slot=value;}
    ~WindowGroupScope(){slot=previous;}
};
