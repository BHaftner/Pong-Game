@echo off
setlocal

:: Compiler and flags
set FLAGS=-ggdb3 -O0 -std=c++17 -Wall

:: Include and library paths
set INCLUDES=-Iinclude -IC:\msys64\mingw64\include
set LIBS=-LC:\msys64\mingw64\lib -lglfw3 -lopengl32 -lgdi32

:: Source and output
set SOURCE=src\main.cpp include\glad.c
set OUTPUT=pong.exe

:: Compile
g++ -o %OUTPUT% %SOURCE% %FLAGS% %INCLUDES% %LIBS%

if %errorlevel% equ 0 (
    echo Compiled successfully.
) else (
    echo Compilation failed.
)

endlocal