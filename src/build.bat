@echo off
rem Сборка DockCalendar под Windows (нужен MinGW-w64 в PATH: g++ и windres)
cd /d "%~dp0"
where windres >nul 2>nul || (echo [!] Не найден windres. Установите MinGW-w64 и добавьте его bin в PATH. & exit /b 1)
where g++ >nul 2>nul || (echo [!] Не найден g++. Установите MinGW-w64 и добавьте его bin в PATH. & exit /b 1)
windres resource.rc -O coff -o resource.res || exit /b 1
g++ -O2 -std=c++17 -municode -mwindows main.cpp resource.res -o ..\DockCalendar.exe -static -lgdiplus -ldwmapi -lshell32 -lgdi32 -luser32 -ladvapi32 -s || exit /b 1
echo [OK] Собрано: ..\DockCalendar.exe
