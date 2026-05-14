@echo off
REM ─────────────────────────────────────────────────────────────────
REM  build.bat  —  Windows build for VO2Max
REM  Run from the folder that contains vo2max.py
REM ─────────────────────────────────────────────────────────────────

echo === Checking Python ===
python --version
if errorlevel 1 (
    echo ERROR: Python not found. Install from https://python.org and add to PATH.
    pause & exit /b 1
)

echo.
echo === Installing / upgrading dependencies ===
python -m pip install --upgrade pip
python -m pip install pyinstaller pyqtgraph PyQt6 pyserial numpy PyOpenGL PyOpenGL-accelerate

echo.
echo === Building executable ===
pyinstaller vo2max.spec --clean --noconfirm

if errorlevel 1 (
    echo.
    echo BUILD FAILED — see errors above.
    pause & exit /b 1
)

echo.
echo ================================================================
echo  Build complete!
echo  Executable folder:  dist\VO2Max\
echo  Launch with:        dist\VO2Max\VO2Max.exe
echo ================================================================
pause
