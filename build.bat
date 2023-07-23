@echo off

cls

set SourceFiles=..\src\Win32Platform.cpp ..\external\imgui\imgui_impl.cpp
set CompilerFlags= -nologo -Gm- -GR- -Oi -EHa- -W4 -wd4201 -wd4100 -wd4996 -wd4063 -FC -Z7 -I ..\external\ -DIS_MSVC=1 -DTARGET_WINDOWS -std:c++20 -arch:AVX2
set LinkerFlags=-opt:ref -incremental:no -debug:full
set Libraries=user32.lib winmm.lib shell32.lib opengl32.lib gdi32.lib

IF "%1"=="-r" (
	set CompilerFlags=%CompilerFlags% -O2 -MT -wd4189
) ELSE (
	set CompilerFlags=%CompilerFlags% -Od -MTd -DDEBUG_BUILD=1
)

IF NOT EXIST .\bin mkdir .\bin

pushd .\bin

set start=%time%
cl %CompilerFlags% %SourceFiles% %Libraries% -link %LinkerFlags%
set end=%time%
IF %ERRORLEVEL% NEQ 0 echo [31mFailed![0m
IF %ERRORLEVEL% EQU 0 echo [32mSuccess[0m

popd

cmd /c timediff.bat Compilation %start% %end%
echo.

echo Generating tags...
ctags -R --use-slash-as-filename-separator=yes .
IF %ERRORLEVEL% NEQ 0 echo Tag generation [31mfailed![0m
IF %ERRORLEVEL% EQU 0 echo Done generating tags
