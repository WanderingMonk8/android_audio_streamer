package com.example.audiocapture

import com.arthenica.ffmpegkit.FFmpegKit
import com.arthenica.ffmpegkit.FFmpegKit
import com.arthenica.ffmpegkit.ReturnCode
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
    private lateinit var mockEncoder: OpusEncoder

    @Before
    fun setup() {
        mockEncoder = mock(OpusEncoder::class.java)
        encodingService = EncodingService().apply {
            // Inject mock encoder
            encoder = mockEncoder
        }
    }

    @After
    fun cleanup() {
        encodingService.release()
    }

    @Test
    fun shouldEncodeAudioFrame_whenGivenValidPcmData() {
        // Arrange
        val testPcmData = ByteArray(480) // 120 frames * 2 channels * 2 bytes (16-bit)
        `when`(mockEncoder.encode(
            any(), anyInt(), anyInt(),
            any(), anyInt(), anyInt()
        )).thenReturn(100) // Simulate encoded size

        // Act
        val result = encodingService.encodeFrame(testPcmData)

        // Assert
        assertNotNull(result)
        assertEquals(100, result?.size)
        verify(mockEncoder).encode(
            eq(testPcmData), eq(0), eq(480),
            any(), eq(0), eq(400)
        )
    }

    @Test
    fun shouldReturnNull_whenEncoderFailsToInitialize() {
        // Arrange
        encodingService.encoder = null
        `when`(mockEncoder.init(anyInt(), anyInt(), anyInt())).thenReturn(false)

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
    fun shouldMaintainLowLatency_whenEncodingFrames() {
        // Arrange
        val testPcmData = ByteArray(480)
        `when`(mockEncoder.encode(any(), anyInt(), anyInt(), any(), anyInt(), anyInt()))
            .thenReturn(100)

        // Act & Measure
        val startTime = System.nanoTime()
        encodingService.encodeFrame(testPcmData)
        val durationMs = (System.nanoTime() - startTime) / 1_000_000

        // Assert
        assertTrue("Encoding took too long: ${durationMs}ms", durationMs < 5)
    }

    @Test
    fun shouldReleaseResources_whenDestroyed() {
        // Act
        encodingService.release()

        // Assert
        verify(mockEncoder).destroy()
        assertNull(encodingService.encoder)
    }
}