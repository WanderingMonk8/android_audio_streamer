#!/usr/bin/env python3
"""
Android Configuration Update Script
Updates Android app configuration with PC IP address for integration testing
"""

import re
import sys
import socket
import subprocess
from pathlib import Path

def get_local_ip():
    """Get the local IP address"""
    try:
        # Connect to a remote address to determine local IP
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"

def update_encoding_service(pc_ip):
    """Update EncodingService.kt with PC IP address"""
    encoding_service_path = Path("app/src/main/java/com/example/audiocapture/EncodingService.kt")
    
    if not encoding_service_path.exists():
        print(f"❌ EncodingService.kt not found at {encoding_service_path}")
        return False
    
    try:
        # Read the file
        content = encoding_service_path.read_text()
        
        # Update the targetHost line
        pattern = r'private val targetHost = "([^"]*)"'
        replacement = f'private val targetHost = "{pc_ip}"'
        
        new_content = re.sub(pattern, replacement, content)
        
        if new_content != content:
            # Write back the updated content
            encoding_service_path.write_text(new_content)
            print(f"✅ Updated EncodingService.kt with PC IP: {pc_ip}")
            return True
        else:
            print(f"⚠️  No targetHost found to update in EncodingService.kt")
            return False
            
    except Exception as e:
        print(f"❌ Error updating EncodingService.kt: {e}")
        return False

def update_integration_test(pc_ip):
    """Update AndroidPCIntegrationTest.kt with PC IP address"""
    test_path = Path("app/src/test/java/com/example/audiocapture/integration/AndroidPCIntegrationTest.kt")
    
    if not test_path.exists():
        print(f"⚠️  Integration test not found at {test_path}")
        return False
    
    try:
        # Read the file
        content = test_path.read_text()
        
        # Update the PC IP in ManualPCIntegrationTest
        pattern = r'val pcIpAddress = "([^"]*)"'
        replacement = f'val pcIpAddress = "{pc_ip}"'
        
        new_content = re.sub(pattern, replacement, content)
        
        if new_content != content:
            # Write back the updated content
            test_path.write_text(new_content)
            print(f"✅ Updated integration test with PC IP: {pc_ip}")
            return True
        else:
            print(f"⚠️  No pcIpAddress found to update in integration test")
            return False
            
    except Exception as e:
        print(f"❌ Error updating integration test: {e}")
        return False

def main():
    print("🔧 Android Configuration Update Tool")
    print("===================================")
    
    # Get PC IP address
    if len(sys.argv) > 1:
        pc_ip = sys.argv[1]
        print(f"📍 Using provided IP address: {pc_ip}")
    else:
        pc_ip = get_local_ip()
        print(f"📍 Auto-detected IP address: {pc_ip}")
    
    # Validate IP address format
    try:
        socket.inet_aton(pc_ip)
    except socket.error:
        print(f"❌ Invalid IP address format: {pc_ip}")
        sys.exit(1)
    
    print(f"\n🎯 Updating Android configuration with PC IP: {pc_ip}")
    
    # Update files
    encoding_updated = update_encoding_service(pc_ip)
    test_updated = update_integration_test(pc_ip)
    
    if encoding_updated or test_updated:
        print("\n✅ Configuration update completed!")
        print("\n📱 Next steps:")
        print("1. Build Android app: ./gradlew assembleDebug")
        print("2. Install on device: ./gradlew installDebug")
        print("3. Start PC receiver: cd pc-receiver/build && ./audio_receiver")
        print("4. Run Android app and test streaming")
        print("\n🧪 To run integration tests:")
        print("./gradlew test --tests AndroidPCIntegrationTest")
        print("./gradlew test --tests ManualPCIntegrationTest")
    else:
        print("\n⚠️  No configuration files were updated")
        print("Please check that the files exist and have the expected format")

if __name__ == "__main__":
    main()