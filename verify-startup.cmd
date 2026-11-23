@echo off
setlocal
if "%~1"=="" exit /b 2
set "TEST_EXE=%~f1"
pushd "%~dp0"
if not exist "build" mkdir "build"
cl /nologo /std:c++17 /EHsc /O2 /MT /W4 "startup-tests.cpp" /Fo"build\startup-tests.obj" /Fe:"build\startup-tests.exe"
if errorlevel 1 goto failed
"build\startup-tests.exe" "%TEST_EXE%"
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
