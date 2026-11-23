TS2-LotEdgeWalls-v0.19 - latest release source - 17 October 2026

Updated compatible-executable build. The original whole-file EXE hash gate
has been replaced by checks of the required game code at the known UC layout.
Gameplay policies and patch logic are unchanged from the preceding no-logging
release. No mod logging has been restored.

This folder is self-contained. It does not need either of the other source
folders to build. Use an x86 MSVC Developer Command Prompt with the Windows SDK:
    build.cmd
Build output: build/TS2-LotEdgeWalls.asi
The build also compiles and runs tests.cpp, the existing gameplay/ABI tests.
Optional mapped-image startup tests against the original reference game EXE:
    verify-startup.cmd "C:\path\to\TheSims2.exe"
Those fixtures expect the 46,208,224-byte original reference EXE. They copy it
into private memory and do not edit the EXE or a running game process.

Matching distributed ASI SHA256:
112DD23E800FCC4CC274A58C12CA4720E6FE1F47FE5960F3AF82F127AB513DB8
A rebuild can have a different file hash due to build timestamps/tool versions.

Compatibility remains limited to the supported 32-bit UC code layout; this is
not a Legacy port or support for arbitrary EXEs. Required code mismatches stop
activation. Wine/Linux and other users' executable variants need in-game testing.

These files contain source/build instructions, not an installable game mod.
Use the separate binary download for ASIs and matching catalogue packages/scripts.
No game binaries, build outputs or runtime logs are included here.
