
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl ..\src\main_runtime.cpp ..\src\common.cpp /ZI /I"." /I"..\dep\tl\include" /I"..\dep\freetype\include" /I"..\dep\stb" /I"..\dep\tgraphics\include" /std:c++latest /D"BUILD_DEBUG=0" /link /LIBPATH:"../dep/freetype/win64" > compile_log.txt
