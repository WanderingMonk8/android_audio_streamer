package com.example.audiocapture

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.util.Log
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean

class OboeWrapper {
    external fun nativeStartCapture(sampleRate: Int, channelCount: Int)
    external fun nativeStopCapture()
    external fun nativeGetAudioBuffer(): ByteBuffer
    
    private var audioRecord: AudioRecord? = null
    private val isUsingFallback = AtomicBoolean(false)
    private val isInitialized = AtomicBoolean(false)
    
    companion object {
        init {
            try {
                System.loadLibrary("native-lib")
            } catch (e: UnsatisfiedLinkError) {
                Log.e("OboeWrapper", "Failed to load native library", e)
            }
        }
    }

    fun start() {
        try {
            nativeStartCapture(48000, 2)
            isInitialized.set(true)
        } catch (e: Exception) {
            Log.w("OboeWrapper", "Native start failed, falling back to AudioRecord", e)
            fallbackToAudioRecord()
        }
    }
    
    fun startCapture(sampleRate: Int, channelCount: Int) {
        // Validate parameters
        if (sampleRate <= 0) {
            throw IllegalArgumentException("Sample rate must be positive")
        }
        if (channelCount <= 0) {
            throw IllegalArgumentException("Channel count must be positive")
        }
        
        try {
            nativeStartCapture(sampleRate, channelCount)
            isInitialized.set(true)
        } catch (e: Exception) {
            Log.w("OboeWrapper", "Native start failed, falling back to AudioRecord", e)
            fallbackToAudioRecord(sampleRate, channelCount)
        }
    }

    fun stop() {
        try {
            if (isUsingFallback.get()) {
                audioRecord?.stop()
                audioRecord?.release()
                audioRecord = null
            } else {
                nativeStopCapture()
            }
            isInitialized.set(false)
            isUsingFallback.set(false)
        } catch (e: Exception) {
            Log.e("OboeWrapper", "Error stopping audio capture", e)
        }
    }

    fun getBuffer(): ByteBuffer? {
        return try {
            if (isUsingFallback.get()) {
                getFallbackBuffer()
            } else {
                nativeGetAudioBuffer()
            }
        } catch (e: Exception) {
            Log.e("OboeWrapper", "Error getting audio buffer", e)
            null
        }
    }
    
    fun getAudioBuffer(): ByteBuffer? = getBuffer()
    
    fun isUsingFallback(): Boolean = isUsingFallback.get()
    
    fun isInitialized(): Boolean = isInitialized.get()

    private fun fallbackToAudioRecord(sampleRate: Int = 48000, channelCount: Int = 2) {
        try {
            Log.w("OboeWrapper", "Falling back to AudioRecord")
            
            val channelConfig = if (channelCount == 1) {
                AudioFormat.CHANNEL_IN_MONO
            } else {
                AudioFormat.CHANNEL_IN_STEREO
            }
            
            val bufferSize = AudioRecord.getMinBufferSize(
                sampleRate,
                channelConfig,
                AudioFormat.ENCODING_PCM_16BIT
            )
            
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC,
                sampleRate,
                channelConfig,
                AudioFormat.ENCODING_PCM_16BIT,
                bufferSize
            )
            
            audioRecord?.startRecording()
            isUsingFallback.set(true)
            isInitialized.set(true)
            
        } catch (e: Exception) {
            Log.e("OboeWrapper", "Failed to initialize AudioRecord fallback", e)
            throw e
        }
    }
    
    private fun getFallbackBuffer(): ByteBuffer? {
        return try {
            audioRecord?.let { record ->
                val bufferSize = 1024 // Small buffer for low latency
                val buffer = ByteArray(bufferSize)
                val bytesRead = record.read(buffer, 0, bufferSize)
                
                if (bytesRead > 0) {
                    ByteBuffer.wrap(buffer, 0, bytesRead)
                } else {
                    null
                }
            }
        } catch (e: Exception) {
            Log.e("OboeWrapper", "Error reading from AudioRecord", e)
            null
        }
    }
}