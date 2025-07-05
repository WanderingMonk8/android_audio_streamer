package com.example.audiocapture

import android.content.Context
import android.media.projection.MediaProjection
import android.util.Log
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

interface AudioCaptureCallback {
    fun onAudioReady(audioData: ByteBuffer, numFrames: Int)
    fun onError(error: String)
}

open class AudioCaptureService(
    private val context: Context,
    private val mediaProjection: MediaProjection,
    private val callback: AudioCaptureCallback? = null,
    private val initializeEncoder: Boolean = true
) {
    private val oboeWrapper = OboeWrapper()
    private val isCapturing = AtomicBoolean(false)
    private val encodingService = EncodingService(context, initializeEncoder)
    private val audioPollingExecutor: ScheduledExecutorService = Executors.newSingleThreadScheduledExecutor()
    
    // Test support fields
    private var testAudioStream: AudioStream? = null
    private var testAudioStreamBuilder: AudioStreamBuilder? = null
    private var testEncodingService: EncodingService? = null

    open fun startCapture() {
        if (isCapturing.get()) return

        try {
            oboeWrapper.nativeStartCapture(48000, 2)
            isCapturing.set(true)
            startAudioPolling()
        } catch (e: Exception) {
            Log.e("AudioCapture", "Failed to start Oboe audio capture", e)
            stopCapture()
            throw e
        }
    }

    open fun stopCapture() {
        if (!isCapturing.getAndSet(false)) return

        try {
            audioPollingExecutor.shutdown()
            oboeWrapper.nativeStopCapture()
            (testEncodingService ?: encodingService).release()
        } catch (e: Exception) {
            Log.e("AudioCapture", "Error stopping Oboe capture", e)
        }
    }
    
    fun isCapturing(): Boolean = isCapturing.get()
    
    private fun startAudioPolling() {
        audioPollingExecutor.scheduleAtFixedRate({
            try {
                val buffer = oboeWrapper.getBuffer()
                buffer?.let { 
                    val numFrames = it.remaining() / (2 * 4) // 2 channels * 4 bytes per float
                    callback?.onAudioReady(it, numFrames)
                    
                    // Process for encoding
                    (testEncodingService ?: encodingService).encodeAudio(it, numFrames)
                }
            } catch (e: Exception) {
                Log.e("AudioCapture", "Error polling audio data", e)
                callback?.onError("Audio polling failed: ${e.message}")
            }
        }, 0, 5, TimeUnit.MILLISECONDS) // Poll every 5ms for low latency
    }
    
    // Audio callback methods for testing compatibility
    fun onAudioReady(@Suppress("UNUSED_PARAMETER") stream: AudioStream?, audioData: ByteBuffer?, numFrames: Int): Boolean {
        return try {
            if (audioData != null && isCapturing.get()) {
                callback?.onAudioReady(audioData, numFrames)
                (testEncodingService ?: encodingService).encodeAudio(audioData, numFrames)
                true
            } else {
                false
            }
        } catch (e: Exception) {
            Log.e("AudioCapture", "Error in onAudioReady", e)
            false
        }
    }
    
    fun onErrorBeforeClose(@Suppress("UNUSED_PARAMETER") stream: AudioStream?, error: Int) {
        Log.e("AudioCapture", "Audio stream error before close: $error")
        isCapturing.set(false)
        callback?.onError("Stream error: $error")
        stopCapture()
    }
    
    fun onErrorAfterClose(@Suppress("UNUSED_PARAMETER") stream: AudioStream?, error: Int) {
        Log.e("AudioCapture", "Audio stream error after close: $error")
        isCapturing.set(false)
        callback?.onError("Stream error after close: $error")
    }
    
    // Test support methods
    fun setAudioStreamForTesting(stream: AudioStream) {
        testAudioStream = stream
    }
    
    fun setAudioStreamBuilderForTesting(builder: AudioStreamBuilder) {
        testAudioStreamBuilder = builder
    }
    
    fun setEncodingServiceForTesting(service: EncodingService) {
        testEncodingService = service
    }
    
    fun setCapturingForTesting(capturing: Boolean) {
        isCapturing.set(capturing)
    }
}