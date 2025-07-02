package com.example.audiocapture

import com.example.audiocapture.encoder.Encoder
import com.example.audiocapture.encoder.FFmpegEncoder
import com.example.audiocapture.network.NetworkService
import java.nio.ByteBuffer
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.RejectedExecutionException

class EncodingService(
    private val initializeEncoder: Boolean = true,
    private val enableNetworking: Boolean = true,
    private val targetHost: String = "192.168.1.100",
    private val targetPort: Int = 12345
) {
    private val sampleRate = 48000
    private val channels = 2
    private val bitrate = "128k"
    
    private var encoder: Encoder? = null
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private var encodingTask: Future<ByteArray>? = null
    
    // Network service for streaming to PC
    private var networkService: NetworkService? = null
    
    // Test support
    var testEncoder: Encoder? = null
    var testNetworkService: NetworkService? = null
    
    init {
        if (initializeEncoder) {
            try {
                encoder = FFmpegEncoder(sampleRate, channels, bitrate)
            } catch (e: Exception) {
                // FFmpeg encoder not available in test environment
                encoder = null
            }
        }
        
        if (enableNetworking) {
            try {
                networkService = NetworkService(targetHost, targetPort)
                networkService?.start()
            } catch (e: Exception) {
                // Network service not available in test environment
                networkService = null
            }
        }
    }

    fun encodeFrame(pcmData: ByteArray, callback: (ByteArray?) -> Unit = {}): ByteArray? {
        val activeEncoder = testEncoder ?: encoder
        if (pcmData.isEmpty() || activeEncoder == null) {
            callback(null)
            return null
        }

        return try {
            encodingTask?.cancel(true)
            val result = activeEncoder.encode(pcmData)
            
            // Send encoded data to PC if available
            result?.let { encodedData ->
                val activeNetworkService = testNetworkService ?: networkService
                activeNetworkService?.sendEncodedAudio(encodedData)
            }
            
            callback(result)
            result
        } catch (e: Exception) {
            callback(null)
            null
        }
    }
    
    fun encodeFrame(pcmData: ByteArray): ByteArray? {
        return encodeFrame(pcmData) {}
    }

    fun encodeAudio(buffer: ByteBuffer, @Suppress("UNUSED_PARAMETER") numFrames: Int) {
        val bytes = ByteArray(buffer.remaining())
        buffer.get(bytes)
        encodeFrame(bytes) { /* Handle encoded packet if needed */ }
    }

    fun release() {
        encodingTask?.cancel(true)
        executor.shutdownNow()
        
        // Stop network service
        try {
            (testNetworkService ?: networkService)?.stop()
        } catch (e: Exception) {
            // Ignore release errors
        }
        
        try {
            (testEncoder ?: encoder)?.destroy()
        } catch (e: Exception) {
            // Ignore release errors
        }
        
        encoder = null
        testEncoder = null
        networkService = null
        testNetworkService = null
    }
    
    // Network management methods
    fun updateNetworkTarget(host: String, port: Int = targetPort): Boolean {
        val activeNetworkService = testNetworkService ?: networkService
        return activeNetworkService?.updateTarget(host, port) ?: false
    }
    
    fun getNetworkStats() = (testNetworkService ?: networkService)?.getStats()
    
    fun isNetworkRunning(): Boolean = (testNetworkService ?: networkService)?.isRunning() ?: false
    
    fun resetNetworkStats() {
        (testNetworkService ?: networkService)?.resetStats()
    }
    
    // Test support methods
    fun setEncoderForTesting(testEncoder: Encoder?) {
        this.testEncoder = testEncoder
    }
    
    fun setNetworkServiceForTesting(testNetworkService: NetworkService?) {
        this.testNetworkService = testNetworkService
    }
    
    fun getEncoderForTesting(): Encoder? {
        return testEncoder ?: encoder
    }
    
    fun getNetworkServiceForTesting(): NetworkService? {
        return testNetworkService ?: networkService
    }
}