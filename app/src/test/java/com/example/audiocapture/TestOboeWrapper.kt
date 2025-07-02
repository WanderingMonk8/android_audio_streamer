package com.example.audiocapture

import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Test-only version of OboeWrapper that doesn't load native libraries
 */
class TestOboeWrapper {
    private val isInitialized = AtomicBoolean(false)
    private val isUsingFallback = AtomicBoolean(false)
    
    fun startCapture(sampleRate: Int, channelCount: Int) {
        // Validate parameters
        if (sampleRate <= 0) {
            throw IllegalArgumentException("Sample rate must be positive")
        }
        if (channelCount <= 0) {
            throw IllegalArgumentException("Channel count must be positive")
        }
        
        isInitialized.set(true)
    }
    
    fun stop() {
        isInitialized.set(false)
        isUsingFallback.set(false)
    }
    
    fun getBuffer(): ByteBuffer? {
        return if (isInitialized.get()) {
            ByteBuffer.allocate(1024)
        } else {
            null
        }
    }
    
    fun isInitialized(): Boolean = isInitialized.get()
    
    fun isUsingFallback(): Boolean = isUsingFallback.get()
}