# WiFi MulticastLock Testing Steps

## 🔍 Step 1: Monitor Logs
Run the log monitoring script to see detailed output:
```bash
# Windows
tmp_rovodev_monitor_logs.bat

# Or manually:
adb logcat -c
adb logcat | findstr /i "UdpSender NetworkService MulticastLock Connection AudioCapture"
```

## 📱 Step 2: Test the Android App
1. Open the AudioCapture app on your Android device
2. Enter your PC IP address (192.168.1.103)
3. Enter port 12345
4. Start streaming

## 🔍 Step 3: What to Look For in Logs

### ✅ Expected SUCCESS logs:
```
UdpSender: Attempting to acquire WiFi MulticastLock...
UdpSender: WifiManager obtained successfully
UdpSender: MulticastLock created with tag: AudioCaptureUDP
UdpSender: MulticastLock.acquire() called
UdpSender: WiFi MulticastLock acquired successfully - isHeld: true
UdpSender: WiFi Info - SSID: "YourWiFiName", IP: 192168001xxx
UdpSender: UDP socket created successfully
UdpSender: UDP sender started successfully
UdpSender: Attempting to send UDP packet...
UdpSender: UDP packet sent successfully!
```

### ❌ Potential FAILURE logs:
```
UdpSender: WifiManager not available - this will likely cause UDP failures!
UdpSender: Failed to acquire WiFi MulticastLock
UdpSender: MulticastLock status: false
UdpSender: Failed to send packet - Connection refused
```

## 🎯 Step 4: Analyze Results

### If MulticastLock is acquired but still getting "Connection refused":
- The issue might be elsewhere (firewall, network routing, etc.)
- Try different approaches (binding to specific interface, etc.)

### If MulticastLock is NOT acquired:
- Check if app has CHANGE_WIFI_MULTICAST_STATE permission
- Check if device is connected to WiFi
- Try on different Android device/version

## 📋 Step 5: Report Results
Please share:
1. The complete log output
2. Whether MulticastLock was acquired (isHeld: true/false)
3. At what point the "Connection refused" error occurs
4. Your Android device model and version
5. WiFi network type (home router, enterprise, etc.)