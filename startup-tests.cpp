#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cassert>
#include <vector>
#include <utility>
#include <fstream>
#include <iterator>
static HMODULE fixture_module;
extern "C" HMODULE WINAPI FixtureGetModuleHandleW(LPCWSTR) {return fixture_module;}
static LPTHREAD_START_ROUTINE queued_start;
static HANDLE WINAPI FixtureCreateThread(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE fn,LPVOID,DWORD,LPDWORD) { queued_start=fn; return nullptr; }
#define CreateThread FixtureCreateThread
#define GetModuleHandleW FixtureGetModuleHandleW
#include "plugin.cpp"
#undef GetModuleHandleW
#undef CreateThread

int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<BYTE> disk((std::istreambuf_iterator<char>(file)),{});
    assert(disk.size()==46208224);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(disk.data()+reinterpret_cast<IMAGE_DOS_HEADER*>(disk.data())->e_lfanew);
    const unsigned size=nt->OptionalHeader.SizeOfImage;
    auto* image=static_cast<BYTE*>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    assert(image); fixture_module=reinterpret_cast<HMODULE>(image);
    auto load=[&]() {
        memset(image,0,size);memcpy(image,disk.data(),nt->OptionalHeader.SizeOfHeaders);
        const auto* sections=IMAGE_FIRST_SECTION(nt);
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)
            memcpy(image+sections[i].VirtualAddress,disk.data()+sections[i].PointerToRawData,sections[i].SizeOfRawData);
        // Model the loader's rebasing of absolute operands in the dependency
        // signatures and the existing vtable checks. No code is executed here.
        for(const auto& s:compatibility::signatures)for(unsigned i=0;i<s.fixup_count;++i) {
            unsigned value;memcpy(&value,image+s.rva+s.fixups[i],4);
            value+=static_cast<unsigned>(reinterpret_cast<uintptr_t>(image))-0x400000u;
            memcpy(image+s.rva+s.fixups[i],&value,4);
        }
        *reinterpret_cast<uintptr_t*>(image+0xed9988)=reinterpret_cast<uintptr_t>(image)+0xc231b0;
        *reinterpret_cast<uintptr_t*>(image+0xed9994)=reinterpret_cast<uintptr_t>(image)+0xc23330;
        initialized=false;
    };
    using Changes=std::vector<std::pair<unsigned,BYTE>>;
    auto start=[&]() {
        std::vector<BYTE> before(image,image+size);
        if(queued_start) {auto fn=queued_start;queued_start=nullptr;fn(nullptr);}else InitializeASI();
        Changes changed;
        for(unsigned i=0;i<size;++i)if(image[i]!=before[i])changed.emplace_back(i,image[i]);
        return changed;
    };
    load();assert(compatibility::accepts(reinterpret_cast<uintptr_t>(image)));
    const Changes baseline=start(); assert(baseline.size()>120);
    assert(image[0x1fe3f3]==0x18 && image[0x1fe405]==0x18 && image[0x20a9a4]==0x8c && image[0x203be5]==0x1c);
    // RPC-style LoadLibrary calls DllMain without invoking the export.
    load();queued_start=nullptr;
    assert(DllMain(fixture_module,DLL_PROCESS_ATTACH,nullptr));assert(queued_start);
    assert(start()==baseline);assert(start().empty());
    // Ultimate ASI Loader can call the export before the queued fallback runs.
    load();assert(DllMain(fixture_module,DLL_PROCESS_ATTACH,nullptr));assert(queued_start);
    InitializeASI();assert(start().empty());
    // Whole-file identity differences must no longer prevent activation.
    load();image[0x30]^=0x5a;
    reinterpret_cast<IMAGE_NT_HEADERS32*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew)->OptionalHeader.CheckSum^=0x12345678;
    image[0x1000]^=0x01; // Unrelated code outside every patch/dependency signature.
    assert(start()==baseline);
    // Reinitialization cannot install a second copy.
    assert(start().empty());
    unsigned negatives=0;
    // Every required native dependency is checked before any patch is written.
    for(const auto& s:compatibility::signatures) {
        load();image[s.rva]^=1;assert(start().empty());++negatives;
    }
    const unsigned patches[]={0x1fe3f1,0x1fe403,0x2032eb,0x2000cf,0x200771,0x2007c3,0xd9a07,0xd9a7c,0xd9a1f,0xd9a91,0x20a9ad,0x20a9d6,0x20a9a3,0x20a9c8,0x203736,0x8239fe,0xcc60a,0xcc613,0xcc764,0xcc772,0xcc780,0xcc78e,0xd8b6a,0xd8bd4,0xd8b7f,0xd8be9,0x203be3,0x4c500b,0x8323e3,0x4c71d4,0x831a67,0x830abf,0x830be4,0x830c14,0x830c45,0x830c70,0x830b90,0x830ba5,0x830a3e,0x83114b,0x83117c,0x8311ab,0x8311e3,0x830ca0,0x830d80,0x83181c,0x830db5,0x83126c,0x831498,0x8314f6,0x83170f,0x83175d};
    for(unsigned rva:patches) {load();image[rva]^=1;assert(start().empty());++negatives;}
    load();reinterpret_cast<IMAGE_NT_HEADERS32*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew)->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
    assert(start().empty());++negatives;
    load();reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew=0x7fffffff;
    assert(start().empty());++negatives;
    load();reinterpret_cast<IMAGE_NT_HEADERS32*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew)->OptionalHeader.SizeOfImage=4096;
    assert(start().empty());++negatives;
    load();assert(start()==baseline);
    assert(!compatibility::accepts(1));
    VirtualFree(image,0,MEM_RELEASE);
    printf("lot-edges: original startup, unrelated executable edits, repeated initialization, and %u incompatible-code/header cases passed; %u changed bytes.\n",negatives,unsigned(baseline.size()));
}
