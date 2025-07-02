package com.example.audiocapture

import com.example.audiocapture.encoder.Encoder
import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer

/**
 * Simple unit tests that don't require Android framework or native libraries
 */
class SimpleUnitTest {
    
    // Create a simple test encoder that doesn't require mocking
    private class TestEncoder : Encoder {
        var encodeCallCount = 0
        var lastInput: ByteArray? = null
        var shouldReturnNull = false
        var destroyed = false
        
        override fun init(sampleRate: Int, channels: Int, frameSize: Int): Boolean = true
        
        override fun encode(input: ByteArray): ByteArray? {
            encodeCallCount++
            lastInput = input
            return if (shouldReturnNull) null else ByteArray(100)
        }
        
        override fun destroy() {
            destroyed = true
        }
    }
    
    @Test
    fun encodingService_shouldEncodeData_whenValidInput() {
        // Arrange
        val encodingService = EncodingService(initializeEncoder = false) // Don't initialize FFmpeg in tests
        val testEncoder = TestEncoder()
        encodingService.setEncoderForTesting(testEncoder)
        
        val testData = ByteArray(480)
        
        // Act
        val result = encodingService.encodeFrame(testData)
        
        // Assert
        assertNotNull(result)
        assertEquals(100, result!!.size)
        assertEquals(1, testEncoder.encodeCallCount)
        assertArrayEquals(testData, testEncoder.lastInput)
    }
    
    @Test
    fun encodingService_shouldReturnNull_whenEmptyInput() {
        // Arrange
        val encodingService = EncodingService(initializeEncoder = false) // Don't initialize FFmpeg in tests
        val testEncoder = TestEncoder()
        encodingService.setEncoderForTesting(testEncoder)
        
        // Act
        val result = encodingService.encodeFrame(ByteArray(0))
        
        // Assert
        assertNull(result)
        assertEquals(0, testEncoder.encodeCallCount) // Should not call encode for empty input
    }
    
    @Test
    fun encodingService_shouldReturnNull_whenNoEncoder() {
        // Arrange
        val encodingService = EncodingService(initializeEncoder = false) // Don't initialize FFmpeg in tests
        encodingService.setEncoderForTesting(null)
        
        // Act
        val result = encodingService.encodeFrame(ByteArray(480))
        
        // Assert
        assertNull(result)
    }
    
    @Test
    fun encodingService_shouldReleaseEncoder_whenReleased() {
        // Arrange
        val encodingService = EncodingService(initializeEncoder = false) // Don't initialize FFmpeg in tests
        val testEncoder = TestEncoder()
        encodingService.setEncoderForTesting(testEncoder)
        
        // Act
        encodingService.release()
        
        // Assert
        assertTrue(testEncoder.destroyed)
        assertNull(encodingService.getEncoderForTesting())
    }
    
    @Test
    fun encodingService_shouldProcessByteBuffer_whenValidBuffer() {
        // Arrange
        val encodingService = EncodingService(initializeEncoder = false) // Don't initialize FFmpeg in tests
        val testEncoder = TestEncoder()
        encodingService.setEncoderForTesting(testEncoder)
        
        val testData = ByteArray(480)
        val buffer = ByteBuffer.wrap(testData)
        
        // Act
        encodingService.encodeAudio(buffer, 120)
        
        // Assert
        assertEquals(1, testEncoder.encodeCallCount)
        assertArrayEquals(testData, testEncoder.lastInput)
    }
    
    @Test
    fun oboeWrapper_shouldValidateParameters() {
        // Use test wrapper that doesn't load native libraries
        val wrapper = TestOboeWrapper()
        
        // Test invalid sample rate
        assertThrows(IllegalArgumentException::class.java) {
            wrapper.startCapture(0, 2)
        }
        
        assertThrows(IllegalArgumentException::class.java) {
            wrapper.startCapture(-1, 2)
        }
        
        // Test invalid channel count
        assertThrows(IllegalArgumentException::class.java) {
            wrapper.startCapture(48000, 0)
        }
        
        assertThrows(IllegalArgumentException::class.java) {
            wrapper.startCapture(48000, -1)
        }
        
        // Test valid parameters
        wrapper.startCapture(48000, 2)
        assertTrue(wrapper.isInitialized())
    }
    
    @Test
    fun oboeWrapper_shouldReturnNullBuffer_whenNotInitialized() {
        val wrapper = TestOboeWrapper()
        
        val buffer = wrapper.getBuffer()
        
        assertNull(buffer)
        assertFalse(wrapper.isInitialized())
    }
    
    @Test
    fun oboeWrapper_shouldReturnBuffer_whenInitialized() {
        val wrapper = TestOboeWrapper()
        
        wrapper.startCapture(48000, 2)
        val buffer = wrapper.getBuffer()
        
        assertNotNull(buffer)
        assertTrue(wrapper.isInitialized())
        
        // Test state tracking
        wrapper.stop()
        assertFalse(wrapper.isInitialized())
        assertFalse(wrapper.isUsingFallback())
    }
}