@echo off
echo ========================================
echo   WiFi MulticastLock Monitoring Script
echo ========================================
echo.
echo This script will monitor Android logs for:
echo - WiFi MulticastLock acquisition/release
echo - UDP socket creation and sending
echo - Connection refused errors
echo.
echo Press Ctrl+C to stop monitoring
echo.
pause

echo Starting log monitoring...
echo.

adb logcat -c
adb logcat | findstr /i "UdpSender NetworkService MulticastLock Connection AudioCapture"