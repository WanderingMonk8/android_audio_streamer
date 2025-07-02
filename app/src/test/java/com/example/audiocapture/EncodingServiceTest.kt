package com.example.audiocapture

import com.example.audiocapture.encoder.Encoder
import com.example.audiocapture.network.NetworkService
import com.example.audiocapture.network.NetworkStats
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
    private lateinit var testNetworkService: TestNetworkService
    
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
    
    // Simple test network service implementation
    private class TestNetworkService : NetworkService("127.0.0.1", 12345) {
        var sendCallCount = 0
        var lastEncodedData: ByteArray? = null
        var isRunningState = false
        var shouldFailStart = false
        
        override fun start(): Boolean {
            return if (shouldFailStart) {
                false
            } else {
                isRunningState = true
                true
            }
        }
        
        override fun stop() {
            isRunningState = false
        }
        
        override fun sendEncodedAudio(encodedData: ByteArray) {
            sendCallCount++
            lastEncodedData = encodedData
        }
        
        override fun isRunning(): Boolean = isRunningState
        
        override fun getStats(): NetworkStats {
            return NetworkStats(
                packetsSent = sendCallCount.toLong(),
                bytesSent = (lastEncodedData?.size ?: 0).toLong(),
                sendErrors = 0,
                queueSize = 0,
                isRunning = isRunningState
            )
        }
        
        override fun resetStats() {
            sendCallCount = 0
            lastEncodedData = null
        }
    }

    @Before
    fun setup() {
        testEncoder = TestEncoder()
        testNetworkService = TestNetworkService()
        encodingService = EncodingService(
            initializeEncoder = false, // Don't initialize FFmpeg in tests
            enableNetworking = false   // Don't initialize real networking in tests
        )
        encodingService.setEncoderForTesting(testEncoder)
        encodingService.setNetworkServiceForTesting(testNetworkService)
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

    @Test
    fun shouldSendEncodedAudio_whenNetworkServiceAvailable() {
        // Arrange
        testNetworkService.isRunningState = true
        val testPcmData = ByteArray(480)

        // Act
        val result = encodingService.encodeFrame(testPcmData)

        // Assert
        assertNotNull(result)
        assertEquals(1, testEncoder.encodeCallCount)
        assertEquals(1, testNetworkService.sendCallCount)
        assertArrayEquals(result, testNetworkService.lastEncodedData)
    }

    @Test
    fun shouldNotSendAudio_whenEncodingFails() {
        // Arrange
        testNetworkService.isRunningState = true
        testEncoder.shouldReturnNull = true
        val testPcmData = ByteArray(480)

        // Act
        val result = encodingService.encodeFrame(testPcmData)

        // Assert
        assertNull(result)
        assertEquals(1, testEncoder.encodeCallCount)
        assertEquals(0, testNetworkService.sendCallCount) // Should not send when encoding fails
    }

    @Test
    fun shouldGetNetworkStats_whenNetworkServiceAvailable() {
        // Arrange
        testNetworkService.isRunningState = true
        testNetworkService.sendCallCount = 5

        // Act
        val stats = encodingService.getNetworkStats()

        // Assert
        assertNotNull(stats)
        assertEquals(5L, stats?.packetsSent)
        assertTrue(stats?.isRunning ?: false)
    }

    @Test
    fun shouldReturnNull_whenNetworkServiceNotAvailable() {
        // Arrange
        encodingService.setNetworkServiceForTesting(null)

        // Act
        val stats = encodingService.getNetworkStats()
        val isRunning = encodingService.isNetworkRunning()

        // Assert
        assertNull(stats)
        assertFalse(isRunning)
    }

    @Test
    fun shouldResetNetworkStats_whenCalled() {
        // Arrange
        testNetworkService.isRunningState = true
        testNetworkService.sendCallCount = 10
        testNetworkService.lastEncodedData = ByteArray(100)

        // Act
        encodingService.resetNetworkStats()

        // Assert
        assertEquals(0, testNetworkService.sendCallCount)
        assertNull(testNetworkService.lastEncodedData)
    }

    @Test
    fun shouldReleaseNetworkService_whenDestroyed() {
        // Arrange
        testNetworkService.isRunningState = true

        // Act
        encodingService.release()

        // Assert
        assertFalse(testNetworkService.isRunningState)
        assertNull(encodingService.getNetworkServiceForTesting())
    }
}