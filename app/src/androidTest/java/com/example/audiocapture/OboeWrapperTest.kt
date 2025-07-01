package com.example.audiocapture

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.mockk
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.nio.ByteBuffer

@RunWith(AndroidJUnit4::class)
class OboeWrapperTest {

    private lateinit var oboeWrapper: OboeWrapper

    @Before
    fun setup() {
        oboeWrapper = OboeWrapper()
    }

    @Test
    fun startCapture_shouldThrow_whenInvalidSampleRate() {
        // Arrange
        val invalidSampleRate = 0
        val channelCount = 2

        // Act & Assert
        assertThrows(IllegalArgumentException::class.java) {
            oboeWrapper.startCapture(invalidSampleRate, channelCount)
        }
    }

    @Test
    fun startCapture_shouldThrow_whenInvalidChannelCount() {
        // Arrange
        val sampleRate = 48000
        val invalidChannelCount = 0

        // Act & Assert
        assertThrows(IllegalArgumentException::class.java) {
            oboeWrapper.startCapture(sampleRate, invalidChannelCount)
        }
    }

    @Test
    fun getAudioBuffer_shouldReturnNull_whenNotInitialized() {
        // Arrange - No initialization

        // Act
        val buffer = oboeWrapper.getAudioBuffer()

        // Assert
        assertNull(buffer)
    }

    @Test
    fun shouldFallbackToAudioRecord_whenNativeFails() {
        // Arrange
        val sampleRate = 48000
        val channelCount = 2
        val mockAudioRecord = mockk<AudioRecord>(relaxed = true)

        // Act
        try {
            oboeWrapper.startCapture(sampleRate, channelCount)
            fail("Expected native failure")
        } catch (e: Exception) {
            // Assert
            assertTrue(oboeWrapper.isUsingFallback())
        }
    }

    @Test
    fun getAudioBuffer_shouldReturnValidBuffer_whenInitialized() {
        // Arrange
        val sampleRate = 48000
        val channelCount = 2
        oboeWrapper.startCapture(sampleRate, channelCount)

        // Act
        val buffer = oboeWrapper.getAudioBuffer()

        // Assert
        assertNotNull(buffer)
        assertTrue(buffer is ByteBuffer)
    }
}