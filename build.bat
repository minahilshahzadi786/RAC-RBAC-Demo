@echo off
echo Building C++ Web Server...
g++ -O3 -std=c++17 main.cpp -o server.exe -lws2_32 -lwsock32
if %errorlevel% neq 0 (
    echo Build failed.
    exit /b %errorlevel%
)
echo Build successful! Run server.exe to start.
