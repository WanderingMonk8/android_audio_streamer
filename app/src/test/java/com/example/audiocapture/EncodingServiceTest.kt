package com.example.audiocapture

import com.example.audiocapture.encoder.Encoder
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.*
import org.robolectric.RobolectricTestRunner
import java.nio.ByteBuffer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class EncodingServiceTest {
    private lateinit var encodingService: EncodingService
    private lateinit var mockEncoder: Encoder

    @Before
    fun setup() {
        mockEncoder = mock(Encoder::class.java)
        encodingService = EncodingService()
        encodingService.encoder = mockEncoder
    }

    @After
    fun cleanup() {
        encodingService.release()
    }

    @Test
    fun shouldEncodeAudioFrame_whenGivenValidPcmData() {
        // Arrange
        val testPcmData = ByteArray(480)
        val encodedData = ByteArray(100)
        `when`(mockEncoder.encodeFrame(testPcmData)).thenReturn(encodedData)

        // Act
        val result = encodingService.encodeFrame(testPcmData)

        // Assert
        assertNotNull(result)
        assertEquals(100, result?.size)
        assertArrayEquals(encodedData, result)
    }

    @Test
    fun shouldReturnNull_whenEncoderFailsToInitialize() {
        // Arrange
        encodingService.encoder = null

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
        verify(mockEncoder).release()
        assertNull(encodingService.encoder)
    }

    // NEW TESTS
    @Test
    fun shouldHandleConcurrentEncoding_whenMultipleFramesSubmitted() {
        // Arrange
        val testData1 = ByteArray(480)
        val testData2 = ByteArray(480)
        val encodedData = ByteArray(100)
        `when`(mockEncoder.encodeFrame(any())).thenReturn(encodedData)
        val latch = CountDownLatch(2)

        // Act
        encodingService.encodeFrame(testData1) { latch.countDown() }
        encodingService.encodeFrame(testData2) { latch.countDown() }

        // Assert
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        verify(mockEncoder, times(2)).encodeFrame(any())
    }

    @Test
    fun shouldHandleEncodingError_whenEncoderThrowsException() {
        // Arrange
        val testData = ByteArray(480)
        `when`(mockEncoder.encodeFrame(testData)).thenThrow(RuntimeException("Encoding error"))
        var errorOccurred = false

        // Act
        encodingService.encodeFrame(testData) { result ->
            if (result == null) errorOccurred = true
        }

        // Assert
        assertTrue(errorOccurred)
    }

    @Test
    fun shouldCancelPendingTasks_whenReleased() {
        // Arrange
        val testData = ByteArray(480)
        val latch = CountDownLatch(1)
        var callbackCalled = false

        // Act
        encodingService.encodeFrame(testData) {
            callbackCalled = true
            latch.countDown()
        }
        encodingService.release()

        // Assert
        assertFalse(callbackCalled)
    }

    @Test
    fun shouldHandleDifferentFrameSizes_whenEncoding() {
        // Arrange
        val smallFrame = ByteArray(120)
        val largeFrame = ByteArray(960)
        val encodedData = ByteArray(100)
        `when`(mockEncoder.encodeFrame(any())).thenReturn(encodedData)

        // Act
        val result1 = encodingService.encodeFrame(smallFrame)
        val result2 = encodingService.encodeFrame(largeFrame)

        // Assert
        assertNotNull(result1)
        assertNotNull(result2)
    }
}