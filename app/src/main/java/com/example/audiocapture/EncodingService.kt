package com.example.audiocapture

import com.example.audiocapture.encoder.Encoder
import com.example.audiocapture.encoder.FFmpegEncoder
import java.nio.ByteBuffer
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.RejectedExecutionException

class EncodingService(private val initializeEncoder: Boolean = true) {
    private val sampleRate = 48000
    private val channels = 2
    private val bitrate = "128k"
    
    private var encoder: Encoder? = null
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private var encodingTask: Future<ByteArray>? = null
    
    // Test support
    var testEncoder: Encoder? = null
    
    init {
        if (initializeEncoder) {
            try {
                encoder = FFmpegEncoder(sampleRate, channels, bitrate)
            } catch (e: Exception) {
                // FFmpeg encoder not available in test environment
                encoder = null
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
        try {
            (testEncoder ?: encoder)?.destroy()
        } catch (e: Exception) {
            // Ignore release errors
        }
        encoder = null
        testEncoder = null
    }
    
    // Test support methods
    fun setEncoderForTesting(testEncoder: Encoder?) {
        this.testEncoder = testEncoder
    }
    
    fun getEncoderForTesting(): Encoder? {
        return testEncoder ?: encoder
    }
}