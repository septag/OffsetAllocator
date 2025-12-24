@echo off

if "%CATCH2_INCLUDE_DIR%"=="" (
    echo ERROR: CATCH2_INCLUDE_DIR environment variable is not set
    exit /b 1
)

if "%CATCH2_LIB_PATH%"=="" (
    echo ERROR: CATCH2_LIB_PATH environment variable is not set
    exit /b 1
)

cl ^
    /Od /MDd /std:c++20 /Zi /EHsc- ^
    /I"%CATCH2_INCLUDE_DIR%" ^
    /D_ITERATOR_DEBUG_LEVEL=2 ^
    offsetAllocatorTests.cpp ^
    /link /LIBPATH:"%CATCH2_LIB_PATH%" catch2d.lib
