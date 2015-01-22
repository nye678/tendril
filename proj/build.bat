@echo off
cls

rem wd4127 - Conditional expression is constant.
rem wd4189 - local variable is initialized but not referenced.
rem wd4201 - nonstandard extension used : nameless struct/union
rem wd4100 - unreferenced formal parameter
rem wd4530 - C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
set warnings=/W4 /WX /wd4127 /wd4189 /wd4201 /wd4100 /wd4530
set preprocessor=/D_CRT_SECURE_NO_WARNINGS
set debugflags=/Z7 /D_debug

call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x64

if not exist ..\build mkdir ..\build
pushd ..\build

if not exist .\debug mkdir .\debug
pushd .\debug

cl %debugflags% %warnings% %preprocessor% ..\..\tendril_ub.cpp