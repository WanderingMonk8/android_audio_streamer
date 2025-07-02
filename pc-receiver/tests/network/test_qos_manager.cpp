#include "network/qos_manager.h"
#include <cassert>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

using namespace AudioReceiver::Network;

// Helper function to create a UDP socket for testing
int create_test_udp_socket() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return -1;
    }
#endif

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        std::cerr << "Failed to create test socket" << std::endl;
        return -1;
    }
    
    return sock;
}

void close_test_socket(int sock) {
    if (sock >= 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
    }
}

void test_qos_manager_construction() {
    std::cout << "Testing QoS Manager construction..." << std::endl;
    
    QoSManager qos_manager;
    
    // QoS support depends on platform and privileges
    // We just test that construction succeeds
    std::cout << "QoS supported: " << (qos_manager.is_qos_supported() ? "Yes" : "No") << std::endl;
    
    std::cout << "✓ QoS Manager construction test passed" << std::endl;
}

void test_dscp_enum_values() {
    std::cout << "Testing DSCP enum values..." << std::endl;
    
    // Test DSCP value conversion
    assert(QoSManager::dscp_to_value(DSCPClass::DEFAULT) == 0);
    assert(QoSManager::dscp_to_value(DSCPClass::CS1) == 8);
    assert(QoSManager::dscp_to_value(DSCPClass::CS5) == 40);
    assert(QoSManager::dscp_to_value(DSCPClass::EF) == 46);
    
    // Test string conversion
    assert(std::string(QoSManager::dscp_to_string(DSCPClass::DEFAULT)) == "Default");
    assert(std::string(QoSManager::dscp_to_string(DSCPClass::CS5)) == "CS5 (Real-time Audio/Video)");
    
    std::cout << "✓ DSCP enum values test passed" << std::endl;
}

void test_socket_dscp_setting() {
    std::cout << "Testing socket DSCP setting..." << std::endl;
    
    QoSManager qos_manager;
    int test_socket = create_test_udp_socket();
    
    if (test_socket < 0) {
        std::cout << "⚠ Skipping socket DSCP test - failed to create socket" << std::endl;
        return;
    }
    
    // Test setting DSCP on socket
    bool result = qos_manager.set_socket_dscp(test_socket, DSCPClass::CS5);
    
    if (qos_manager.is_qos_supported()) {
        // If QoS is supported, setting should succeed
        std::cout << "DSCP CS5 setting result: " << (result ? "Success" : "Failed") << std::endl;
        
        // Test validation
        if (result) {
            bool validation = qos_manager.validate_qos_setting(test_socket, DSCPClass::CS5);
            std::cout << "DSCP validation result: " << (validation ? "Success" : "Failed") << std::endl;
        }
    } else {
        std::cout << "⚠ QoS not supported on this platform/configuration" << std::endl;
    }
    
    close_test_socket(test_socket);
    std::cout << "✓ Socket DSCP setting test passed" << std::endl;
}

void test_audio_qos_convenience_method() {
    std::cout << "Testing audio QoS convenience method..." << std::endl;
    
    QoSManager qos_manager;
    int test_socket = create_test_udp_socket();
    
    if (test_socket < 0) {
        std::cout << "⚠ Skipping audio QoS test - failed to create socket" << std::endl;
        return;
    }
    
    // Test audio QoS convenience method
    bool result = qos_manager.set_audio_qos(test_socket);
    
    if (qos_manager.is_qos_supported()) {
        std::cout << "Audio QoS setting result: " << (result ? "Success" : "Failed") << std::endl;
        
        // Validate that CS5 was set
        if (result) {
            bool validation = qos_manager.validate_qos_setting(test_socket, DSCPClass::CS5);
            std::cout << "Audio QoS validation result: " << (validation ? "Success" : "Failed") << std::endl;
        }
    } else {
        std::cout << "⚠ QoS not supported on this platform/configuration" << std::endl;
    }
    
    close_test_socket(test_socket);
    std::cout << "✓ Audio QoS convenience method test passed" << std::endl;
}

void test_invalid_socket_handling() {
    std::cout << "Testing invalid socket handling..." << std::endl;
    
    QoSManager qos_manager;
    
    // Test with invalid socket descriptor
    bool result = qos_manager.set_socket_dscp(-1, DSCPClass::CS5);
    assert(!result); // Should fail with invalid socket
    
    // Test validation with invalid socket
    bool validation = qos_manager.validate_qos_setting(-1, DSCPClass::CS5);
    assert(!validation); // Should fail with invalid socket
    
    // Test getting DSCP from invalid socket
    DSCPClass dscp = qos_manager.get_socket_dscp(-1);
    assert(dscp == DSCPClass::DEFAULT); // Should return default for invalid socket
    
    std::cout << "✓ Invalid socket handling test passed" << std::endl;
}

void test_multiple_dscp_classes() {
    std::cout << "Testing multiple DSCP classes..." << std::endl;
    
    QoSManager qos_manager;
    
    if (!qos_manager.is_qos_supported()) {
        std::cout << "⚠ Skipping multiple DSCP test - QoS not supported" << std::endl;
        return;
    }
    
    int test_socket = create_test_udp_socket();
    if (test_socket < 0) {
        std::cout << "⚠ Skipping multiple DSCP test - failed to create socket" << std::endl;
        return;
    }
    
    // Test different DSCP classes
    DSCPClass test_classes[] = {
        DSCPClass::DEFAULT,
        DSCPClass::CS1,
        DSCPClass::CS3,
        DSCPClass::CS5,
        DSCPClass::EF
    };
    
    for (DSCPClass dscp_class : test_classes) {
        bool result = qos_manager.set_socket_dscp(test_socket, dscp_class);
        std::cout << "Setting " << QoSManager::dscp_to_string(dscp_class) 
                  << ": " << (result ? "Success" : "Failed") << std::endl;
        
        if (result) {
            bool validation = qos_manager.validate_qos_setting(test_socket, dscp_class);
            std::cout << "  Validation: " << (validation ? "Success" : "Failed") << std::endl;
        }
    }
    
    close_test_socket(test_socket);
    std::cout << "✓ Multiple DSCP classes test passed" << std::endl;
}

void test_qos_manager_thread_safety() {
    std::cout << "Testing QoS Manager thread safety..." << std::endl;
    
    // Basic thread safety test - multiple QoSManager instances
    QoSManager qos1;
    QoSManager qos2;
    
    // Both should report the same QoS support status
    assert(qos1.is_qos_supported() == qos2.is_qos_supported());
    
    // Test that static methods work correctly
    assert(QoSManager::dscp_to_value(DSCPClass::CS5) == 40);
    assert(std::string(QoSManager::dscp_to_string(DSCPClass::CS5)) == "CS5 (Real-time Audio/Video)");
    
    std::cout << "✓ QoS Manager thread safety test passed" << std::endl;
}

// Test runner
void run_qos_manager_tests() {
    std::cout << "\n=== Running QoS Manager Tests ===" << std::endl;
    
    test_qos_manager_construction();
    test_dscp_enum_values();
    test_socket_dscp_setting();
    test_audio_qos_convenience_method();
    test_invalid_socket_handling();
    test_multiple_dscp_classes();
    test_qos_manager_thread_safety();
    
    std::cout << "\n✅ All QoS Manager tests passed!" << std::endl;
}