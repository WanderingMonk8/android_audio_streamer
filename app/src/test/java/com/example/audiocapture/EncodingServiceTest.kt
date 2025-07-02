package com.example.audiocapture

import com.example.audiocapture.encoder.Encoder
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [33], manifest = Config.NONE)
class EncodingServiceTest {
    private lateinit var encodingService: EncodingService
    private lateinit var testEncoder: TestEncoder
    
    // Simple test encoder implementation
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

    @Before
    fun setup() {
        testEncoder = TestEncoder()
        encodingService = EncodingService(initializeEncoder = false) // Don't initialize FFmpeg in tests
        encodingService.setEncoderForTesting(testEncoder)
    }

    @After
    fun cleanup() {
        encodingService.release()
    }

    @Test
    fun shouldEncodeAudioFrame_whenGivenValidPcmData() {
        // Arrange
        val testPcmData = ByteArray(480)

        // Act
        val result = encodingService.encodeFrame(testPcmData)

        // Assert
        assertNotNull(result)
        assertEquals(100, result?.size)
        assertEquals(1, testEncoder.encodeCallCount)
        assertArrayEquals(testPcmData, testEncoder.lastInput)
    }

    @Test
    fun shouldReturnNull_whenEncoderFailsToInitialize() {
        // Arrange
        encodingService.setEncoderForTesting(null)

        // Act
        val result = encodingService.encodeFrame(ByteArray(480))

        // Assert
        assertNull(result)
    }

    @Test
    fun shouldHandleEmptyFrame_whenGivenZeroBytes() {
        // Arrange
        val emptyPcmData = ByteArray(0)

        // Act
        val result = encodingService.encodeFrame(emptyPcmData)

        // Assert
        assertNull(result)
    }

    @Test
    fun shouldReleaseResources_whenDestroyed() {
        // Act
        encodingService.release()

        // Assert
        assertTrue(testEncoder.destroyed)
        assertNull(encodingService.getEncoderForTesting())
    }

    @Test
    fun shouldHandleDifferentFrameSizes_whenEncoding() {
        // Arrange
        val smallFrame = ByteArray(120)
        val largeFrame = ByteArray(960)

        // Act
        val result1 = encodingService.encodeFrame(smallFrame)
        val result2 = encodingService.encodeFrame(largeFrame)

        // Assert
        assertNotNull(result1)
        assertNotNull(result2)
        assertEquals(2, testEncoder.encodeCallCount)
        assertArrayEquals(largeFrame, testEncoder.lastInput) // Last input should be the large frame
    }
}