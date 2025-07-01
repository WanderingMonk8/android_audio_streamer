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
}