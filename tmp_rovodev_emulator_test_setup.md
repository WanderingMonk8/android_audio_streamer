# 🎯 Android Emulator UDP Test Setup

## 📱 **Step 1: Create Android Emulator**

### **Option A: Using Android Studio**
1. Open Android Studio
2. Go to **Tools → AVD Manager**
3. Click **Create Virtual Device**
4. Choose a device (e.g., Pixel 4, Pixel 6)
5. Select **API Level 30+ (Android 11+)** for best compatibility
6. **Important**: Choose **x86_64** image (faster than ARM)
7. Click **Finish**

### **Option B: Using Command Line**
```bash
# List available system images
sdkmanager --list | findstr "system-images"

# Create AVD
avdmanager create avd -n "UDPTest" -k "system-images;android-30;google_apis;x86_64"

# Start emulator
emulator -avd UDPTest
```

## 🌐 **Step 2: Configure Emulator Networking**

### **Emulator Network Setup:**
- **Emulator IP**: Usually `10.0.2.15` (internal)
- **Host PC IP**: `10.0.2.2` (from emulator's perspective)
- **Port forwarding**: May be needed for PC → Emulator communication

### **Set up Port Forwarding:**
```bash
# Forward PC port 12345 to emulator port 12345
adb -s emulator-5554 forward tcp:12345 tcp:12345
```

## 🔧 **Step 3: Test Configuration**

### **Test 1: Emulator → PC (Your Current Setup)**
- **PC Receiver**: Run on `0.0.0.0:12345` (listen on all interfaces)
- **Android App**: Connect to `10.0.2.2:12345` (host PC from emulator)
- **Expected**: Should work if it's device-specific

### **Test 2: PC → Emulator (Reverse Test)**
- **Android App**: Listen on `0.0.0.0:12345`
- **PC Sender**: Connect to `localhost:12345` (via port forwarding)
- **Expected**: Confirms bidirectional UDP

## 📋 **Step 4: Testing Procedure**

### **4.1 Install App on Emulator**
```bash
# Install APK
adb -s emulator-5554 install app/build/outputs/apk/debug/app-debug.apk

# Check installation
adb -s emulator-5554 shell pm list packages | findstr audiocapture
```

### **4.2 Start PC Receiver**
```bash
cd pc-receiver
# Compile and run PC receiver
cmake -B build
cmake --build build
./build/main
```

### **4.3 Monitor Emulator Logs**
```bash
# Monitor logs in real-time
adb -s emulator-5554 logcat | findstr "UdpSender NetworkService MulticastLock AudioCapture"
```

### **4.4 Test App**
1. Open AudioCapture app in emulator
2. Enter IP: `10.0.2.2` (host PC)
3. Enter Port: `12345`
4. Start streaming
5. Watch logs for UDP success/failure

## 🎯 **Expected Results**

### **If UDP Works in Emulator:**
```
✅ UdpSender: Socket created successfully!
✅ UdpSender: UDP packet sent successfully!
✅ PC Receiver: Received audio packet
```
**Conclusion**: Your physical device has UDP restrictions

### **If UDP Fails in Emulator:**
```
❌ UdpSender: socket failed: ECONNREFUSED
```
**Conclusion**: Broader Android/app-level issue

## 🔄 **Alternative Emulator Configurations**

### **Try Different API Levels:**
- **API 28 (Android 9)**: Fewer restrictions
- **API 30 (Android 11)**: Current target
- **API 33 (Android 13)**: Latest

### **Try Different Emulator Types:**
- **Google APIs**: Standard emulator
- **Google Play**: Play Store enabled
- **AOSP**: Pure Android (no Google services)

## 🚀 **Quick Start Commands**

```bash
# 1. Start emulator
emulator -avd UDPTest

# 2. Set up port forwarding
adb forward tcp:12345 tcp:12345

# 3. Install app
adb install app-debug.apk

# 4. Monitor logs
adb logcat | findstr "UdpSender"

# 5. Start PC receiver
cd pc-receiver && ./build/main
```

## 💡 **Troubleshooting**

### **If Emulator is Slow:**
- Enable **Hardware Acceleration** (HAXM/Hyper-V)
- Allocate more **RAM** (4GB+)
- Use **x86_64** images (not ARM)

### **If Network Issues:**
- Try **Cold Boot** emulator
- Check **Windows Firewall** settings
- Use **Wireshark** to monitor traffic

## 🎯 **Next Steps After Testing**

### **If Emulator Works:**
- Confirms device-specific UDP blocking
- Consider alternative devices or protocols

### **If Emulator Fails:**
- Investigate app-level restrictions
- Try native socket implementation
- Consider WebRTC DataChannel

**Ready to test? Let me know if you need help with any of these steps!**