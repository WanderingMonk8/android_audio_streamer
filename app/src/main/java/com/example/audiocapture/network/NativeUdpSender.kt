package com.example.audiocapture.network

import android.util.Log

/**
 * Native UDP sender that bypasses Java DatagramSocket restrictions
 * Uses JNI to create UDP sockets in native C++ code
 */
class NativeUdpSender(
    private val targetHost: String,
    private val targetPort: Int
) {
    private var isRunning = false
    
    companion object {
        private const val TAG = "NativeUdpSender"
        
        init {
            System.loadLibrary("native-lib")
        }
    }
    
    // Native methods
    private external fun createSocket(host: String, port: Int): Int
    private external fun sendPacket(data: ByteArray): Int
    private external fun closeSocket()
    
    fun start(): Boolean {
        Log.i(TAG, "Starting native UDP sender to $targetHost:$targetPort")
        
        val result = createSocket(targetHost, targetPort)
        if (result < 0) {
            Log.e(TAG, "Failed to create native UDP socket")
            return false
        }
        
        isRunning = true
        Log.i(TAG, "Native UDP sender started successfully")
        return true
    }
    
    fun sendPacket(packet: AudioPacket): Boolean {
        if (!isRunning) {
            Log.w(TAG, "Cannot send packet - sender not running")
            return false
        }
        
        return try {
            val data = packet.serialize()
            val result = sendPacket(data)
            
            if (result > 0) {
                Log.v(TAG, "Sent packet via native UDP - Seq: ${packet.sequenceId}, Size: ${data.size} bytes")
                true
            } else {
                Log.e(TAG, "Failed to send packet via native UDP")
                false
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending packet via native UDP", e)
            false
        }
    }
    
    fun stop() {
        if (!isRunning) return
        
        Log.i(TAG, "Stopping native UDP sender")
        closeSocket()
        isRunning = false
        Log.i(TAG, "Native UDP sender stopped")
    }
    
    fun isRunning(): Boolean = isRunning
}