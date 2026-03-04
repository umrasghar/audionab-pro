@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
    echo VCVARS FAILED
    exit /b 1
)
echo VCVARS OK

cd /d D:\audionab-pro
if not exist build mkdir build
cd build

echo Running CMake configure...
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo CMAKE CONFIGURE FAILED
    exit /b 1
)
echo CMake configure OK

echo Building...
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build .
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

echo.
echo =============================
echo BUILD_SUCCESS
echo =============================
dir /b *.exe 2>NUL
