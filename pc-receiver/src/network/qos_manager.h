#pragma once

#include <cstdint>

namespace AudioReceiver {
namespace Network {

/**
 * DSCP (Differentiated Services Code Point) values for QoS marking
 * Based on RFC 4594 - Configuration Guidelines for DiffServ Service Classes
 */
enum class DSCPClass : uint8_t {
    DEFAULT = 0,        // Best effort (000000)
    CS1 = 8,           // Class Selector 1 (001000) - Low priority
    CS2 = 16,          // Class Selector 2 (010000) - Standard
    CS3 = 24,          // Class Selector 3 (011000) - High
    CS4 = 32,          // Class Selector 4 (100000) - Real-time data
    CS5 = 40,          // Class Selector 5 (101000) - Real-time audio/video
    CS6 = 48,          // Class Selector 6 (110000) - Network control
    CS7 = 56,          // Class Selector 7 (111000) - Reserved
    EF = 46,           // Expedited Forwarding (101110) - Low latency
    AF11 = 10,         // Assured Forwarding 1.1 (001010)
    AF21 = 18,         // Assured Forwarding 2.1 (010010)
    AF31 = 26,         // Assured Forwarding 3.1 (011010)
    AF41 = 34          // Assured Forwarding 4.1 (100010)
};

/**
 * QoS Manager for network traffic prioritization
 * Handles DSCP marking for UDP sockets to improve audio streaming performance
 * 
 * Features:
 * - DSCP marking with CS5 (recommended for real-time audio)
 * - Cross-platform socket option setting
 * - QoS capability detection
 * - Traffic class validation
 */
class QoSManager {
public:
    QoSManager();
    ~QoSManager() = default;

    // Non-copyable
    QoSManager(const QoSManager&) = delete;
    QoSManager& operator=(const QoSManager&) = delete;

    /**
     * Set DSCP marking on a UDP socket
     * @param socket_fd Socket file descriptor (Windows: SOCKET, Unix: int)
     * @param dscp_class DSCP class to apply
     * @return true if QoS marking was successfully applied
     */
    bool set_socket_dscp(int socket_fd, DSCPClass dscp_class);

    /**
     * Set DSCP marking for real-time audio (CS5)
     * Convenience method for audio streaming applications
     * @param socket_fd Socket file descriptor
     * @return true if audio QoS marking was successfully applied
     */
    bool set_audio_qos(int socket_fd);

    /**
     * Check if QoS/DSCP marking is supported on this platform
     * @return true if QoS marking is available
     */
    bool is_qos_supported() const;

    /**
     * Get the current DSCP value set on a socket
     * @param socket_fd Socket file descriptor
     * @return DSCP class currently set, or DEFAULT if unable to determine
     */
    DSCPClass get_socket_dscp(int socket_fd) const;

    /**
     * Validate that DSCP marking is working
     * @param socket_fd Socket file descriptor
     * @param expected_dscp Expected DSCP class
     * @return true if the socket has the expected DSCP marking
     */
    bool validate_qos_setting(int socket_fd, DSCPClass expected_dscp) const;

    /**
     * Get human-readable description of DSCP class
     * @param dscp_class DSCP class to describe
     * @return String description of the DSCP class
     */
    static const char* dscp_to_string(DSCPClass dscp_class);

    /**
     * Convert DSCP enum to raw value for socket options
     * @param dscp_class DSCP class
     * @return Raw DSCP value (0-63)
     */
    static uint8_t dscp_to_value(DSCPClass dscp_class);

private:
    bool qos_supported_;
    
    // Platform-specific implementation helpers
    bool set_dscp_windows(int socket_fd, uint8_t dscp_value);
    bool set_dscp_unix(int socket_fd, uint8_t dscp_value);
    bool get_dscp_windows(int socket_fd, uint8_t& dscp_value) const;
    bool get_dscp_unix(int socket_fd, uint8_t& dscp_value) const;
    
    // Constants
    static constexpr DSCPClass AUDIO_DSCP_CLASS = DSCPClass::CS5;
    static constexpr uint8_t DSCP_MASK = 0x3F; // 6 bits for DSCP (0-63)
};

} // namespace Network
} // namespace AudioReceiver