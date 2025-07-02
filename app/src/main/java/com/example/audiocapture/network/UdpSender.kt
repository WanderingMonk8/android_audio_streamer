package com.example.audiocapture.network

import android.util.Log
import kotlinx.coroutines.*
import java.net.*
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

/**
 * UDP sender for audio packets
 * Implements low-latency packet transmission to PC receiver
 */
class UdpSender(
    private val targetHost: String,
    private val targetPort: Int
) {
    private var socket: DatagramSocket? = null
    private var targetAddress: InetSocketAddress? = null
    private val isRunning = AtomicBoolean(false)
    
    // Statistics
    private val packetsSent = AtomicLong(0)
    private val bytesSent = AtomicLong(0)
    private val sendErrors = AtomicLong(0)
    
    // Coroutine scope for async operations
    private val senderScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    
    companion object {
        private const val TAG = "UdpSender"
        private const val SOCKET_TIMEOUT_MS = 5000
        private const val MAX_PACKET_SIZE = 2048
    }
    
    /**
     * Start the UDP sender
     */
    fun start(): Boolean {
        if (isRunning.get()) {
            Log.w(TAG, "UDP sender already running")
            return false
        }
        
        return try {
            // Resolve target address
            targetAddress = InetSocketAddress(targetHost, targetPort)
            
            // Create UDP socket
            socket = DatagramSocket().apply {
                soTimeout = SOCKET_TIMEOUT_MS
                // Set socket options for low latency
                trafficClass = 0x10 // Low delay
                sendBufferSize = 64 * 1024 // 64KB buffer
            }
            
            isRunning.set(true)
            Log.i(TAG, "UDP sender started - target: $targetHost:$targetPort")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start UDP sender", e)
            cleanup()
            false
        }
    }
    
    /**
     * Stop the UDP sender
     */
    fun stop() {
        if (!isRunning.getAndSet(false)) {
            return
        }
        
        Log.i(TAG, "Stopping UDP sender...")
        
        // Cancel all coroutines
        senderScope.cancel()
        
        cleanup()
        
        Log.i(TAG, "UDP sender stopped")
    }
    
    /**
     * Send audio packet asynchronously
     */
    fun sendPacketAsync(packet: AudioPacket): Job? {
        if (!isRunning.get()) {
            Log.w(TAG, "Cannot send packet - sender not running")
            return null
        }
        
        return senderScope.launch {
            sendPacketInternal(packet)
        }
    }
    
    /**
     * Send audio packet synchronously
     */
    fun sendPacket(packet: AudioPacket): Boolean {
        if (!isRunning.get()) {
            Log.w(TAG, "Cannot send packet - sender not running")
            return false
        }
        
        return sendPacketInternal(packet)
    }
    
    /**
     * Internal packet sending implementation
     */
    private fun sendPacketInternal(packet: AudioPacket): Boolean {
        return try {
            val data = packet.serialize()
            
            if (data.size > MAX_PACKET_SIZE) {
                Log.w(TAG, "Packet too large: ${data.size} bytes")
                sendErrors.incrementAndGet()
                return false
            }
            
            val datagramPacket = DatagramPacket(
                data, 
                data.size, 
                targetAddress
            )
            
            socket?.send(datagramPacket)
            
            // Update statistics
            packetsSent.incrementAndGet()
            bytesSent.addAndGet(data.size.toLong())
            
            Log.v(TAG, "Sent packet - Seq: ${packet.sequenceId}, Size: ${data.size} bytes")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send packet", e)
            sendErrors.incrementAndGet()
            false
        }
    }
    
    /**
     * Get statistics
     */
    fun getPacketsSent(): Long = packetsSent.get()
    fun getBytesSent(): Long = bytesSent.get()
    fun getSendErrors(): Long = sendErrors.get()
    fun isRunning(): Boolean = isRunning.get()
    
    /**
     * Reset statistics
     */
    fun resetStats() {
        packetsSent.set(0)
        bytesSent.set(0)
        sendErrors.set(0)
    }
    
    /**
     * Cleanup resources
     */
    private fun cleanup() {
        try {
            socket?.close()
        } catch (e: Exception) {
            Log.w(TAG, "Error closing socket", e)
        }
        socket = null
        targetAddress = null
    }
}