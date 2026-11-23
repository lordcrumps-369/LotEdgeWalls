@echo off
setlocal
pushd "%~dp0"
if not exist "build" mkdir "build"
cl /nologo /std:c++17 /EHsc /O2 /MT /W4 /LD "plugin.cpp" /Fo"build\plugin.obj" /link /DEF:"exports.def" /OUT:"build\TS2-LotEdgeWalls.asi" /IMPLIB:"build\plugin.lib"
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /MT /W4 "tests.cpp" /Fo"build\tests.obj" /Fe:"build\tests.exe"
if errorlevel 1 goto failed
build\tests.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
