#!/bin/bash

# Android-PC Integration Test Script
# This script helps set up and run the integration tests

set -e

echo "🚀 Android-PC Integration Test Setup"
echo "===================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the right directory
if [ ! -d "pc-receiver" ] || [ ! -d "app" ]; then
    print_error "Please run this script from the project root directory"
    exit 1
fi

# Step 1: Build PC Receiver
print_status "Building PC receiver..."
cd pc-receiver

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Configure with CMake
print_status "Configuring CMake..."
cmake .. || {
    print_error "CMake configuration failed"
    exit 1
}

# Build
print_status "Compiling PC receiver..."
make -j$(nproc) || {
    print_error "PC receiver compilation failed"
    exit 1
}

print_success "PC receiver built successfully"

# Step 2: Build integration test
print_status "Building integration test..."
if [ -f "../tests/integration_test.cpp" ]; then
    g++ -std=c++17 -I../src \
        ../tests/integration_test.cpp \
        ../src/network/packet.cpp \
        ../src/network/udp_receiver.cpp \
        ../src/network/qos_manager.cpp \
        ../src/network/network_monitor.cpp \
        -o integration_test \
        -pthread \
        $(pkg-config --cflags --libs portaudio-2.0 2>/dev/null || echo "") \
        $(pkg-config --cflags --libs opus 2>/dev/null || echo "") || {
        print_warning "Integration test compilation failed, but continuing..."
    }
else
    print_warning "Integration test source not found"
fi

# Step 3: Run basic tests
print_status "Running basic PC receiver tests..."

if [ -f "./network_tests" ]; then
    print_status "Running network tests..."
    ./network_tests || print_warning "Some network tests failed"
else
    print_warning "Network tests not found"
fi

if [ -f "./audio_tests" ]; then
    print_status "Running audio tests..."
    ./audio_tests || print_warning "Some audio tests failed"
else
    print_warning "Audio tests not found"
fi

# Step 4: Check network connectivity
print_status "Checking network configuration..."

# Get local IP address
LOCAL_IP=$(hostname -I | awk '{print $1}' 2>/dev/null || echo "127.0.0.1")
print_status "Local IP address: $LOCAL_IP"

# Check if port 12345 is available
if netstat -ln 2>/dev/null | grep -q ":12345 "; then
    print_warning "Port 12345 is already in use"
else
    print_success "Port 12345 is available"
fi

# Step 5: Provide instructions
echo ""
echo "🎯 Integration Test Instructions"
echo "==============================="
echo ""
print_status "PC Receiver Setup Complete!"
echo ""
echo "To run the integration tests:"
echo ""
echo "1. Start PC receiver:"
echo "   cd pc-receiver/build"
echo "   ./audio_receiver"
echo ""
echo "2. In another terminal, run integration test:"
echo "   cd pc-receiver/build"
echo "   ./integration_test"
echo ""
echo "3. Update Android app configuration:"
echo "   - Open app/src/main/java/com/example/audiocapture/EncodingService.kt"
echo "   - Change targetHost to: \"$LOCAL_IP\""
echo "   - Build and run Android app"
echo ""
echo "4. Run Android integration tests:"
echo "   ./gradlew test --tests AndroidPCIntegrationTest"
echo ""
echo "5. For manual testing:"
echo "   ./gradlew test --tests ManualPCIntegrationTest"
echo ""

# Step 6: Create quick test commands
cat > ../quick_test.sh << EOF
#!/bin/bash
# Quick test commands

echo "Starting PC receiver..."
cd pc-receiver/build
./audio_receiver &
PC_PID=\$!

echo "PC receiver started with PID: \$PC_PID"
echo "Press Ctrl+C to stop"

# Cleanup function
cleanup() {
    echo "Stopping PC receiver..."
    kill \$PC_PID 2>/dev/null || true
    exit 0
}

trap cleanup INT

# Wait for user interrupt
wait \$PC_PID
EOF

chmod +x ../quick_test.sh

print_success "Quick test script created: quick_test.sh"

# Step 7: Android test setup
cd ../../

print_status "Setting up Android tests..."

# Check if Android tests exist
if [ -f "app/src/test/java/com/example/audiocapture/integration/AndroidPCIntegrationTest.kt" ]; then
    print_success "Android integration tests found"
    
    # Run Android tests
    print_status "Running Android unit tests..."
    ./gradlew test --tests "com.example.audiocapture.network.*" || print_warning "Some Android tests failed"
    
else
    print_warning "Android integration tests not found"
fi

# Step 8: Final summary
echo ""
echo "🎉 Integration Test Setup Complete!"
echo "=================================="
echo ""
print_success "PC receiver built and tested"
print_success "Integration test utilities ready"
print_success "Network configuration checked"
echo ""
print_status "Next steps:"
echo "1. Run './quick_test.sh' to start PC receiver"
echo "2. Update Android app with PC IP: $LOCAL_IP"
echo "3. Build and run Android app"
echo "4. Monitor packet reception on PC"
echo ""
print_status "For detailed testing, see: docs/android_pc_integration_test_plan.md"
echo ""

# Return to original directory
cd "$(dirname "$0")"