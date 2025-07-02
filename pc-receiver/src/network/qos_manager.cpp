#include "network/qos_manager.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <qos2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#endif

namespace AudioReceiver {
namespace Network {

QoSManager::QoSManager() : qos_supported_(false) {
    // Check if QoS is supported on this platform
#ifdef _WIN32
    // On Windows, QoS is generally supported but may require admin privileges
    qos_supported_ = true;
#else
    // On Unix-like systems, check if we can set IP_TOS
    int test_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (test_sock >= 0) {
        int tos = 0;
        socklen_t tos_len = sizeof(tos);
        if (getsockopt(test_sock, IPPROTO_IP, IP_TOS, &tos, &tos_len) == 0) {
            qos_supported_ = true;
        }
        close(test_sock);
    }
#endif
}

bool QoSManager::set_socket_dscp(int socket_fd, DSCPClass dscp_class) {
    if (socket_fd < 0) {
        return false;
    }
    
    uint8_t dscp_value = dscp_to_value(dscp_class);
    
#ifdef _WIN32
    return set_dscp_windows(socket_fd, dscp_value);
#else
    return set_dscp_unix(socket_fd, dscp_value);
#endif
}

bool QoSManager::set_audio_qos(int socket_fd) {
    return set_socket_dscp(socket_fd, AUDIO_DSCP_CLASS);
}

bool QoSManager::is_qos_supported() const {
    return qos_supported_;
}

DSCPClass QoSManager::get_socket_dscp(int socket_fd) const {
    if (socket_fd < 0) {
        return DSCPClass::DEFAULT;
    }
    
    uint8_t dscp_value = 0;
    bool success = false;
    
#ifdef _WIN32
    success = get_dscp_windows(socket_fd, dscp_value);
#else
    success = get_dscp_unix(socket_fd, dscp_value);
#endif
    
    if (!success) {
        return DSCPClass::DEFAULT;
    }
    
    // Convert raw value back to enum
    // Note: This is a simplified conversion - in practice, you might want
    // a more comprehensive mapping
    switch (dscp_value & DSCP_MASK) {
        case 0: return DSCPClass::DEFAULT;
        case 8: return DSCPClass::CS1;
        case 16: return DSCPClass::CS2;
        case 24: return DSCPClass::CS3;
        case 32: return DSCPClass::CS4;
        case 40: return DSCPClass::CS5;
        case 48: return DSCPClass::CS6;
        case 56: return DSCPClass::CS7;
        case 46: return DSCPClass::EF;
        case 10: return DSCPClass::AF11;
        case 18: return DSCPClass::AF21;
        case 26: return DSCPClass::AF31;
        case 34: return DSCPClass::AF41;
        default: return DSCPClass::DEFAULT;
    }
}

bool QoSManager::validate_qos_setting(int socket_fd, DSCPClass expected_dscp) const {
    DSCPClass actual_dscp = get_socket_dscp(socket_fd);
    return actual_dscp == expected_dscp;
}

const char* QoSManager::dscp_to_string(DSCPClass dscp_class) {
    switch (dscp_class) {
        case DSCPClass::DEFAULT: return "Default";
        case DSCPClass::CS1: return "CS1 (Low Priority)";
        case DSCPClass::CS2: return "CS2 (Standard)";
        case DSCPClass::CS3: return "CS3 (High)";
        case DSCPClass::CS4: return "CS4 (Real-time Data)";
        case DSCPClass::CS5: return "CS5 (Real-time Audio/Video)";
        case DSCPClass::CS6: return "CS6 (Network Control)";
        case DSCPClass::CS7: return "CS7 (Reserved)";
        case DSCPClass::EF: return "EF (Expedited Forwarding)";
        case DSCPClass::AF11: return "AF11 (Assured Forwarding 1.1)";
        case DSCPClass::AF21: return "AF21 (Assured Forwarding 2.1)";
        case DSCPClass::AF31: return "AF31 (Assured Forwarding 3.1)";
        case DSCPClass::AF41: return "AF41 (Assured Forwarding 4.1)";
        default: return "Unknown";
    }
}

uint8_t QoSManager::dscp_to_value(DSCPClass dscp_class) {
    return static_cast<uint8_t>(dscp_class) & DSCP_MASK;
}

bool QoSManager::set_dscp_windows(int socket_fd, uint8_t dscp_value) {
#ifdef _WIN32
    // On Windows, we use the IP_TOS socket option
    // The DSCP value needs to be shifted left by 2 bits to account for ECN
    int tos = (dscp_value << 2) & 0xFC;
    
    if (setsockopt(socket_fd, IPPROTO_IP, IP_TOS, 
                   reinterpret_cast<const char*>(&tos), sizeof(tos)) == 0) {
        return true;
    }
    
    // If IP_TOS fails, try Windows-specific QoS API (requires more setup)
    // For now, we'll just return false and log the error
    std::cerr << "Failed to set DSCP on Windows socket (may require admin privileges)" << std::endl;
    return false;
#else
    (void)socket_fd;
    (void)dscp_value;
    return false;
#endif
}

bool QoSManager::set_dscp_unix(int socket_fd, uint8_t dscp_value) {
#ifndef _WIN32
    // On Unix-like systems, use IP_TOS socket option
    // The DSCP value needs to be shifted left by 2 bits to account for ECN
    int tos = (dscp_value << 2) & 0xFC;
    
    if (setsockopt(socket_fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == 0) {
        return true;
    }
    
    std::cerr << "Failed to set DSCP on Unix socket (may require root privileges)" << std::endl;
    return false;
#else
    (void)socket_fd;
    (void)dscp_value;
    return false;
#endif
}

bool QoSManager::get_dscp_windows(int socket_fd, uint8_t& dscp_value) const {
#ifdef _WIN32
    int tos = 0;
    socklen_t tos_len = sizeof(tos);
    
    if (getsockopt(socket_fd, IPPROTO_IP, IP_TOS, 
                   reinterpret_cast<char*>(&tos), &tos_len) == 0) {
        // Extract DSCP value (shift right by 2 bits to remove ECN)
        dscp_value = static_cast<uint8_t>((tos >> 2) & DSCP_MASK);
        return true;
    }
    
    return false;
#else
    (void)socket_fd;
    dscp_value = 0;
    return false;
#endif
}

bool QoSManager::get_dscp_unix(int socket_fd, uint8_t& dscp_value) const {
#ifndef _WIN32
    int tos = 0;
    socklen_t tos_len = sizeof(tos);
    
    if (getsockopt(socket_fd, IPPROTO_IP, IP_TOS, &tos, &tos_len) == 0) {
        // Extract DSCP value (shift right by 2 bits to remove ECN)
        dscp_value = static_cast<uint8_t>((tos >> 2) & DSCP_MASK);
        return true;
    }
    
    return false;
#else
    (void)socket_fd;
    dscp_value = 0;
    return false;
#endif
}

} // namespace Network
} // namespace AudioReceiver