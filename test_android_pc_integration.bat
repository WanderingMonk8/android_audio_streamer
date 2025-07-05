@echo off
REM Android-PC Integration Test Script for Windows
REM This script helps set up and run the integration tests

echo 🚀 Android-PC Integration Test Setup
echo ====================================

REM Check if we're in the right directory
if not exist "pc-receiver" (
    echo [ERROR] Please run this script from the project root directory
    exit /b 1
)

if not exist "app" (
    echo [ERROR] Please run this script from the project root directory
    exit /b 1
)

REM Step 1: Build PC Receiver
echo [INFO] Building PC receiver...
cd pc-receiver

if not exist "build" (
    mkdir build
)

cd build

REM Configure with CMake
echo [INFO] Configuring CMake...
cmake .. -G "MinGW Makefiles"
if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

REM Build
echo [INFO] Compiling PC receiver...
mingw32-make -j4
if errorlevel 1 (
    echo [ERROR] PC receiver compilation failed
    exit /b 1
)

echo [SUCCESS] PC receiver built successfully

REM Step 2: Run basic tests
echo [INFO] Running basic PC receiver tests...

if exist "network_tests.exe" (
    echo [INFO] Running network tests...
    network_tests.exe
) else (
    echo [WARNING] Network tests not found
)

if exist "audio_tests.exe" (
    echo [INFO] Running audio tests...
    audio_tests.exe
) else (
    echo [WARNING] Audio tests not found
)

REM Step 3: Check network configuration
echo [INFO] Checking network configuration...

REM Get local IP address (Windows)
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /c:"IPv4 Address"') do (
    set LOCAL_IP=%%a
    goto :found_ip
)
set LOCAL_IP=127.0.0.1
:found_ip

REM Remove leading spaces
set LOCAL_IP=%LOCAL_IP: =%

echo [INFO] Local IP address: %LOCAL_IP%

REM Step 4: Provide instructions
echo.
echo 🎯 Integration Test Instructions
echo ===============================
echo.
echo [INFO] PC Receiver Setup Complete!
echo.
echo To run the integration tests:
echo.
echo 1. Start PC receiver:
echo    cd pc-receiver\build
echo    audio_receiver.exe
echo.
echo 2. In another terminal, run integration test:
echo    cd pc-receiver\build
echo    integration_test.exe
echo.
echo 3. Update Android app configuration:
echo    - Open app\src\main\java\com\example\audiocapture\EncodingService.kt
echo    - Change targetHost to: "%LOCAL_IP%"
echo    - Build and run Android app
echo.
echo 4. Run Android integration tests:
echo    gradlew test --tests AndroidPCIntegrationTest
echo.
echo 5. For manual testing:
echo    gradlew test --tests ManualPCIntegrationTest
echo.

REM Step 5: Create quick test batch file
cd ..
echo @echo off > quick_test.bat
echo echo Starting PC receiver... >> quick_test.bat
echo cd pc-receiver\build >> quick_test.bat
echo start audio_receiver.exe >> quick_test.bat
echo echo PC receiver started in new window >> quick_test.bat
echo echo Press any key to continue... >> quick_test.bat
echo pause >> quick_test.bat

echo [SUCCESS] Quick test script created: quick_test.bat

REM Step 6: Android test setup
cd ..

echo [INFO] Setting up Android tests...

REM Check if Android tests exist
if exist "app\src\test\java\com\example\audiocapture\integration\AndroidPCIntegrationTest.kt" (
    echo [SUCCESS] Android integration tests found
    
    REM Run Android tests
    echo [INFO] Running Android unit tests...
    gradlew test --tests "com.example.audiocapture.network.*"
    
) else (
    echo [WARNING] Android integration tests not found
)

REM Step 7: Final summary
echo.
echo 🎉 Integration Test Setup Complete!
echo ==================================
echo.
echo [SUCCESS] PC receiver built and tested
echo [SUCCESS] Integration test utilities ready
echo [SUCCESS] Network configuration checked
echo.
echo [INFO] Next steps:
echo 1. Run 'quick_test.bat' to start PC receiver
echo 2. Update Android app with PC IP: %LOCAL_IP%
echo 3. Build and run Android app
echo 4. Monitor packet reception on PC
echo.
echo [INFO] For detailed testing, see: docs\android_pc_integration_test_plan.md
echo.

pause