@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
if not exist bin mkdir bin
cl /std:c11 /experimental:c11atomics /O2 /W3 /Ic /D_CRT_SECURE_NO_WARNINGS c\globals.c c\bus.c c\mod1.c c\mod2.c c\main.c c\rust_stub.c /Fe:bin\poc-c-only.exe
