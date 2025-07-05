package com.example.audiocapture.network

import android.content.Context
import android.util.Log
import kotlinx.coroutines.*
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.ConcurrentLinkedQueue

/**
 * Network service for streaming encoded audio to PC receiver
 * Integrates with EncodingService to transmit Opus packets via UDP
 */
open class NetworkService(
    private val context: Context,
    private val targetHost: String = "192.168.1.100", // Default PC IP
    private val targetPort: Int = 12345
) {
    private var udpSender: UdpSender? = null
    private val sequenceCounter = AtomicLong(0)
    private val packetQueue = ConcurrentLinkedQueue<AudioPacket>()
    
    // Coroutine scope for network operations
    private val networkScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var queueProcessorJob: Job? = null
    
    companion object {
        private const val TAG = "NetworkService"
        private const val MAX_QUEUE_SIZE = 100
        private const val QUEUE_PROCESS_INTERVAL_MS = 1L // Process every 1ms for low latency
    }
    
    /**
     * Start network service
     */
    open fun start(): Boolean {
        Log.i(TAG, "Starting network service...")
        
        udpSender = UdpSender(context, targetHost, targetPort)
        
        if (!udpSender!!.start()) {
            Log.e(TAG, "Failed to start UDP sender")
            return false
        }
        
        // Start packet queue processor
        queueProcessorJob = networkScope.launch {
            processPacketQueue()
        }
        
        Log.i(TAG, "Network service started - streaming to $targetHost:$targetPort")
        return true
    }
    
    /**
     * Stop network service
     */
    open fun stop() {
        Log.i(TAG, "Stopping network service...")
        
        // Cancel queue processor
        queueProcessorJob?.cancel()
        queueProcessorJob = null
        
        // Stop UDP sender
        udpSender?.stop()
        udpSender = null
        
        // Clear packet queue
        packetQueue.clear()
        
        // Cancel all network operations
        networkScope.cancel()
        
        Log.i(TAG, "Network service stopped")
    }
    
    /**
     * Send encoded audio data
     * Called by EncodingService when audio is encoded
     */
    open fun sendEncodedAudio(encodedData: ByteArray) {
        if (udpSender?.isRunning() != true) {
            Log.w(TAG, "Cannot send audio - network service not running")
            return
        }
        
        if (encodedData.isEmpty()) {
            Log.w(TAG, "Ignoring empty encoded data")
            return
        }
        
        val timestamp = System.nanoTime().toULong()
        val sequenceId = sequenceCounter.incrementAndGet().toUInt()
        
        val packet = AudioPacket(sequenceId, timestamp, encodedData)
        
        // Add to queue for processing
        if (packetQueue.size < MAX_QUEUE_SIZE) {
            packetQueue.offer(packet)
            Log.v(TAG, "Queued packet - Seq: $sequenceId, Size: ${encodedData.size} bytes")
        } else {
            Log.w(TAG, "Packet queue full, dropping packet")
        }
    }
    
    /**
     * Process packet queue in background
     */
    private suspend fun processPacketQueue() {
        while (networkScope.isActive) {
            try {
                val packet = packetQueue.poll()
                if (packet != null) {
                    udpSender?.sendPacketAsync(packet)
                } else {
                    // No packets to process, wait briefly
                    delay(QUEUE_PROCESS_INTERVAL_MS)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error processing packet queue", e)
                delay(10) // Brief delay on error
            }
        }
    }
    
    /**
     * Update target PC address
     */
    fun updateTarget(host: String, port: Int = targetPort): Boolean {
        Log.i(TAG, "Updating target to $host:$port")
        
        val wasRunning = udpSender?.isRunning() == true
        
        if (wasRunning) {
            stop()
        }
        
        // Create new service with updated target
        val newService = NetworkService(context, host, port)
        
        if (wasRunning) {
            return newService.start()
        }
        
        return true
    }
    
    /**
     * Get network statistics
     */
    open fun getStats(): NetworkStats {
        val sender = udpSender
        return if (sender != null) {
            NetworkStats(
                packetsSent = sender.getPacketsSent(),
                bytesSent = sender.getBytesSent(),
                sendErrors = sender.getSendErrors(),
                queueSize = packetQueue.size,
                isRunning = sender.isRunning()
            )
        } else {
            NetworkStats()
        }
    }
    
    /**
     * Reset statistics
     */
    open fun resetStats() {
        udpSender?.resetStats()
    }
    
    /**
     * Check if service is running
     */
    open fun isRunning(): Boolean = udpSender?.isRunning() == true
}

/**
 * Network statistics data class
 */
data class NetworkStats(
    val packetsSent: Long = 0,
    val bytesSent: Long = 0,
    val sendErrors: Long = 0,
    val queueSize: Int = 0,
    val isRunning: Boolean = false
) {
    fun getPacketsPerSecond(durationSeconds: Long): Double {
        return if (durationSeconds > 0) packetsSent.toDouble() / durationSeconds else 0.0
    }
    
    fun getBytesPerSecond(durationSeconds: Long): Double {
        return if (durationSeconds > 0) bytesSent.toDouble() / durationSeconds else 0.0
    }
    
    fun getErrorRate(): Double {
        val total = packetsSent + sendErrors
        return if (total > 0) sendErrors.toDouble() / total else 0.0
    }
}