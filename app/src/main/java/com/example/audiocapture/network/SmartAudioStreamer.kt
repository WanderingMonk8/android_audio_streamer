package com.example.audiocapture.network

import android.content.Context
import android.util.Log

/**
 * Smart audio streamer that automatically selects the best available UDP protocol
 * Priority: Raw UDP > Native UDP > Root UDP Bypass
 */
class SmartAudioStreamer(
    private val context: Context,
    private val targetHost: String,
    private val targetPort: Int
) {
    private var udpSender: UdpSender? = null
    private var nativeUdpSender: NativeUdpSender? = null
    private var rootUdpSender: RootUdpSender? = null
    private var activeProtocol: Protocol = Protocol.NONE
    
    enum class Protocol {
        UDP,           // Raw Java UDP (~5ms latency)
        NATIVE_UDP,    // Native C++ UDP (~5ms latency) 
        ROOT_UDP,      // Root-based UDP bypass (~5ms latency)
        NONE
    }
    
    companion object {
        private const val TAG = "SmartAudioStreamer"
    }
    
    /**
     * Start streaming with automatic protocol selection
     */
    fun start(): Boolean {
        Log.i(TAG, "Starting smart audio streamer to $targetHost:$targetPort")
        
        // Try protocols in order of preference (lowest latency first)
        if (tryRawUDP()) {
            activeProtocol = Protocol.UDP
            Log.i(TAG, "Using Raw UDP (Java) - ~5ms latency")
            return true
        }
        
        if (tryNativeUDP()) {
            activeProtocol = Protocol.NATIVE_UDP
            Log.i(TAG, "Using Native UDP (C++) - ~5ms latency")
            return true
        }
        
        // Try root UDP bypass as last resort
        if (tryRootUDP()) {
            activeProtocol = Protocol.ROOT_UDP
            Log.i(TAG, "Using Root UDP bypass - ~5ms latency")
            return true
        }
        
        Log.e(TAG, "All protocols failed - no streaming available")
        return false
    }
    
    /**
     * Send audio packet using active protocol
     */
    fun sendPacket(packet: AudioPacket): Boolean {
        return when (activeProtocol) {
            Protocol.UDP -> udpSender?.sendPacket(packet) ?: false
            Protocol.NATIVE_UDP -> nativeUdpSender?.sendPacket(packet) ?: false
            Protocol.ROOT_UDP -> rootUdpSender?.sendPacket(packet) ?: false
            Protocol.NONE -> {
                Log.w(TAG, "No active protocol - cannot send packet")
                false
            }
        }
    }
    
    /**
     * Stop streaming and cleanup resources
     */
    fun stop() {
        Log.i(TAG, "Stopping smart audio streamer")
        
        udpSender?.stop()
        udpSender = null
        
        nativeUdpSender?.stop()
        nativeUdpSender = null
        
        rootUdpSender?.stop()
        rootUdpSender = null
        
        activeProtocol = Protocol.NONE
        Log.i(TAG, "Smart audio streamer stopped")
    }
    
    /**
     * Get current active protocol
     */
    fun getActiveProtocol(): Protocol = activeProtocol
    
    /**
     * Get protocol description for UI display
     */
    fun getProtocolDescription(): String {
        return when (activeProtocol) {
            Protocol.UDP -> "Raw UDP (Java) - ~5ms latency"
            Protocol.NATIVE_UDP -> "Native UDP (C++) - ~5ms latency"
            Protocol.ROOT_UDP -> "Root UDP bypass - ~5ms latency"
            Protocol.NONE -> "No active protocol"
        }
    }
    
    /**
     * Check if streaming is active
     */
    fun isRunning(): Boolean {
        return when (activeProtocol) {
            Protocol.UDP -> udpSender?.isRunning() ?: false
            Protocol.NATIVE_UDP -> nativeUdpSender?.isRunning() ?: false
            Protocol.ROOT_UDP -> rootUdpSender?.isRunning() ?: false
            Protocol.NONE -> false
        }
    }
    
    private fun tryRawUDP(): Boolean {
        return try {
            Log.d(TAG, "Trying Raw UDP (Java)...")
            udpSender = UdpSender(context, targetHost, targetPort)
            val success = udpSender!!.start()
            if (!success) {
                udpSender = null
            }
            success
        } catch (e: Exception) {
            Log.d(TAG, "Raw UDP failed: ${e.message}")
            udpSender = null
            false
        }
    }
    
    private fun tryNativeUDP(): Boolean {
        return try {
            Log.d(TAG, "Trying Native UDP (C++)...")
            nativeUdpSender = NativeUdpSender(targetHost, targetPort)
            val success = nativeUdpSender!!.start()
            if (!success) {
                nativeUdpSender = null
            }
            success
        } catch (e: Exception) {
            Log.d(TAG, "Native UDP failed: ${e.message}")
            nativeUdpSender = null
            false
        }
    }
    
    
    private fun tryRootUDP(): Boolean {
        return try {
            Log.d(TAG, "Trying Root UDP bypass...")
            rootUdpSender = RootUdpSender(context, targetHost, targetPort)
            val success = rootUdpSender!!.start()
            if (!success) {
                rootUdpSender = null
            }
            success
        } catch (e: Exception) {
            Log.d(TAG, "Root UDP failed: ${e.message}")
            rootUdpSender = null
            false
        }
    }
}