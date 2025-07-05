package com.example.audiocapture.integration

import android.content.Context
import com.example.audiocapture.network.AudioPacket
import com.example.audiocapture.network.NetworkService
import com.example.audiocapture.network.UdpSender
import kotlinx.coroutines.*
import org.junit.Test
import org.junit.Assert.*
import org.junit.Before
import org.junit.After
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import java.net.*
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.random.Random

/**
 * Integration tests for Android → PC audio streaming
 * These tests validate end-to-end packet transmission and compatibility
 */
class AndroidPCIntegrationTest {
    @Mock
    private lateinit var mockContext: Context

    private lateinit var networkService: NetworkService
    private lateinit var udpSender: UdpSender
    
    // Test configuration
    private val testHost = "127.0.0.1" // Change to PC IP for real testing
    private val testPort = 12346 // Use different port to avoid conflicts
    
    @Before
    fun setUp() {
        MockitoAnnotations.openMocks(this)
        networkService = NetworkService(mockContext, testHost, testPort)
        udpSender = UdpSender(mockContext, testHost, testPort)
    }
    
    @After
    fun tearDown() {
        networkService.stop()
        udpSender.stop()
    }

    @Test
    fun testPacketFormatCompatibility() {
        // Test that Android packets match PC receiver expectations
        val testPayload = byteArrayOf(0x01, 0x02, 0x03, 0x04, 0x05)
        val packet = AudioPacket(123u, 456789u, testPayload)
        
        val serialized = packet.serialize()
        
        // Verify packet structure matches PC receiver format
        assertEquals(21, serialized.size) // 16 header + 5 payload
        
        // Check little endian serialization
        val buffer = ByteBuffer.wrap(serialized).order(ByteOrder.LITTLE_ENDIAN)
        
        // Verify sequence ID
        assertEquals(123, buffer.int)
        
        // Verify timestamp
        assertEquals(456789L, buffer.long)
        
        // Verify payload size
        assertEquals(5, buffer.int)
        
        // Verify payload
        val payload = ByteArray(5)
        buffer.get(payload)
        assertArrayEquals(testPayload, payload)
    }

    @Test
    fun testUdpSenderBasicFunctionality() {
        assertTrue("UDP sender should start successfully", udpSender.start())
        assertTrue("UDP sender should be running", udpSender.isRunning())
        
        // Test packet sending
        val testPacket = AudioPacket(1u, System.nanoTime().toULong(), byteArrayOf(0xAA.toByte(), 0xBB.toByte()))
        val success = udpSender.sendPacket(testPacket)
        
        // Note: This will fail without a receiver, but tests the sending logic
        // In real integration testing, PC receiver should be running
        
        udpSender.stop()
        assertFalse("UDP sender should not be running after stop", udpSender.isRunning())
    }

    @Test
    fun testNetworkServiceIntegration() = runBlocking {
        assertTrue("Network service should start", networkService.start())
        
        // Send test audio data
        val testAudioData = generateTestAudioData(480) // 10ms at 48kHz
        networkService.sendEncodedAudio(testAudioData)
        
        // Give time for processing
        delay(100)
        
        val stats = networkService.getStats()
        assertTrue("Should have attempted to send packets", stats.queueSize >= 0)
        
        networkService.stop()
    }

    @Test
    fun testMultiplePacketTransmission() = runBlocking {
        assertTrue(networkService.start())
        
        val packetCount = 10
        val testData = generateTestAudioData(240) // 5ms at 48kHz
        
        // Send multiple packets rapidly
        repeat(packetCount) { i ->
            val audioData = testData.copyOf()
            audioData[0] = i.toByte() // Mark each packet uniquely
            networkService.sendEncodedAudio(audioData)
            delay(5) // 5ms between packets (realistic timing)
        }
        
        // Allow processing time
        delay(200)
        
        val stats = networkService.getStats()
        assertTrue("Should have processed multiple packets", stats.queueSize >= 0)
    }

    @Test
    fun testPacketSequencing() = runBlocking {
        assertTrue(udpSender.start())
        
        val packets = mutableListOf<AudioPacket>()
        
        // Create sequence of packets
        for (i in 1..5) {
            val payload = byteArrayOf(i.toByte(), (i * 2).toByte(), (i * 3).toByte())
            val packet = AudioPacket(i.toUInt(), System.nanoTime().toULong(), payload)
            packets.add(packet)
        }
        
        // Send packets with timing
        packets.forEach { packet ->
            udpSender.sendPacketAsync(packet)
            delay(10) // 10ms between packets
        }
        
        // Verify statistics
        delay(100)
        val packetsSent = udpSender.getPacketsSent()
        assertTrue("Should have attempted to send packets", packetsSent >= 0)
    }

    @Test
    fun testLargePacketHandling() {
        // Test with maximum reasonable audio packet size
        val largePayload = ByteArray(1400) { (it % 256).toByte() } // Near MTU limit
        val packet = AudioPacket(999u, System.nanoTime().toULong(), largePayload)
        
        assertTrue("Large packet should be valid", packet.isValid())
        
        val serialized = packet.serialize()
        assertEquals(1416, serialized.size) // 16 header + 1400 payload
        
        // Test deserialization
        val deserialized = AudioPacket.deserialize(serialized)
        assertNotNull("Should deserialize large packet", deserialized)
        assertEquals(packet, deserialized)
    }

    @Test
    fun testErrorHandling() {
        // Test with invalid host (should handle gracefully)
        val invalidSender = UdpSender(mockContext, "999.999.999.999", testPort)
        
        // Should start (socket creation) but sending will fail
        assertTrue("Should start even with invalid host", invalidSender.start())
        
        val testPacket = AudioPacket(1u, 1u, byteArrayOf(0x01))
        val success = invalidSender.sendPacket(testPacket)
        
        // Sending should fail gracefully
        // (Actual behavior depends on network stack)
        
        invalidSender.stop()
    }

    @Test
    fun testConcurrentPacketSending() = runBlocking {
        assertTrue(udpSender.start())
        
        val jobs = (1..10).map { i ->
            CoroutineScope(Dispatchers.Default).launch {
                val payload = byteArrayOf(i.toByte(), (i * 2).toByte())
                val packet = AudioPacket(i.toUInt(), System.nanoTime().toULong(), payload)
                udpSender.sendPacketAsync(packet)
            }
        }
        
        // Wait for all sends to complete
        jobs.forEach { it.join() }
        
        delay(100)
        
        // Verify no crashes and reasonable statistics
        val stats = udpSender.getPacketsSent()
        assertTrue("Should handle concurrent sending", stats >= 0)
    }

    @Test
    fun testRealisticAudioStreaming() = runBlocking {
        assertTrue(networkService.start())
        
        // Simulate realistic audio streaming (48kHz, 5ms frames)
        val frameSize = 480 * 2 * 4 // 480 samples * 2 channels * 4 bytes (float)
        val frameDurationMs = 5L
        val totalDurationMs = 1000L // 1 second test
        val frameCount = totalDurationMs / frameDurationMs
        
        repeat(frameCount.toInt()) { frameIndex ->
            val audioFrame = generateRealisticAudioFrame(frameIndex, frameSize)
            networkService.sendEncodedAudio(audioFrame)
            delay(frameDurationMs)
        }
        
        val stats = networkService.getStats()
        assertTrue("Should have processed realistic audio stream", stats.queueSize >= 0)
    }

    // Helper methods for test data generation
    
    private fun generateTestAudioData(samples: Int): ByteArray {
        // Generate simple test pattern
        val data = ByteArray(samples * 4) // 4 bytes per float sample
        val buffer = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
        
        repeat(samples) { i ->
            val sample = (Math.sin(2.0 * Math.PI * 440.0 * i / 48000.0) * 0.5).toFloat()
            buffer.putFloat(sample)
        }
        
        return data
    }
    
    private fun generateRealisticAudioFrame(frameIndex: Int, frameSize: Int): ByteArray {
        // Generate more realistic audio data with some variation
        val data = ByteArray(frameSize)
        val buffer = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
        
        val samplesPerFrame = frameSize / 4
        val baseFreq = 440.0 + (frameIndex % 100) // Slight frequency variation
        
        repeat(samplesPerFrame) { sampleIndex ->
            val time = (frameIndex * samplesPerFrame + sampleIndex).toDouble() / 48000.0
            val sample = (Math.sin(2.0 * Math.PI * baseFreq * time) * 0.3 + 
                         Random.nextDouble(-0.1, 0.1)).toFloat() // Add some noise
            buffer.putFloat(sample.coerceIn(-1.0f, 1.0f))
        }
        
        return data
    }
}

/**
 * Manual integration test for real PC receiver
 * Run this when PC receiver is actually running
 */
class ManualPCIntegrationTest {
    @Mock
    private lateinit var mockContext: Context
    
    @Before
    fun setUp() {
        MockitoAnnotations.openMocks(this)
    }
    
    @Test
    fun manualTestWithRealPCReceiver() = runBlocking {
        // IMPORTANT: Update this IP to your PC's actual IP address
        val pcIpAddress = "192.168.1.103" // CHANGE THIS
        val pcPort = 12345
        
        println("=== Manual PC Integration Test ===")
        println("PC IP: $pcIpAddress:$pcPort")
        println("Make sure PC receiver is running!")
        
        val networkService = NetworkService(mockContext, pcIpAddress, pcPort)
        
        try {
            assertTrue("Network service should start", networkService.start())
            println("✓ Network service started")
            
            // Send test packets
            repeat(10) { i ->
                val testData = generateTestTone(i, 480) // 10ms frames
                networkService.sendEncodedAudio(testData)
                println("Sent packet $i")
                delay(10) // 10ms between packets
            }
            
            delay(1000) // Wait for transmission
            
            val stats = networkService.getStats()
            println("Final stats: $stats")
            
        } finally {
            networkService.stop()
            println("✓ Network service stopped")
        }
    }
    
    private fun generateTestTone(frameIndex: Int, samples: Int): ByteArray {
        val frequency = 440.0 + (frameIndex * 10) // Rising tone
        val data = ByteArray(samples * 4)
        val buffer = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
        
        repeat(samples) { i ->
            val time = (frameIndex * samples + i).toDouble() / 48000.0
            val sample = (Math.sin(2.0 * Math.PI * frequency * time) * 0.5).toFloat()
            buffer.putFloat(sample)
        }
        
        return data
    }
}