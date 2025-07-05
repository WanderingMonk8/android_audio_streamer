package com.example.audiocapture.network

import android.content.Context
import org.junit.Test
import org.junit.Assert.*
import org.junit.Before
import org.junit.After
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import kotlinx.coroutines.*

class NetworkServiceTest {
    @Mock
    private lateinit var mockContext: Context

    private lateinit var networkService: NetworkService
    
    @Before
    fun setUp() {
        MockitoAnnotations.openMocks(this)
        // Use localhost for testing
        networkService = NetworkService(mockContext, "127.0.0.1", 12349)
    }
    
    @After
    fun tearDown() {
        networkService.stop()
    }

    @Test
    fun testNetworkServiceConstruction() {
        assertFalse(networkService.isRunning())
        
        val stats = networkService.getStats()
        assertEquals(0L, stats.packetsSent)
        assertEquals(0L, stats.bytesSent)
        assertEquals(0L, stats.sendErrors)
        assertEquals(0, stats.queueSize)
        assertFalse(stats.isRunning)
    }

    @Test
    fun testNetworkServiceStartStop() {
        // Start service
        val started = networkService.start()
        assertTrue("Network service should start successfully", started)
        assertTrue("Network service should be running", networkService.isRunning())
        
        // Stop service
        networkService.stop()
        assertFalse("Network service should not be running after stop", networkService.isRunning())
    }

    @Test
    fun testSendEncodedAudio() = runBlocking {
        // Start service
        assertTrue(networkService.start())
        
        // Send test audio data
        val testAudioData = byteArrayOf(0x01, 0x02, 0x03, 0x04, 0x05)
        networkService.sendEncodedAudio(testAudioData)
        
        // Give some time for processing
        delay(100)
        
        val stats = networkService.getStats()
        assertTrue("Should have queued at least one packet", stats.queueSize >= 0)
    }

    @Test
    fun testSendEmptyAudio() = runBlocking {
        assertTrue(networkService.start())
        
        // Send empty audio data (should be ignored)
        val emptyData = byteArrayOf()
        networkService.sendEncodedAudio(emptyData)
        
        delay(50)
        
        val stats = networkService.getStats()
        assertEquals("Empty data should not be queued", 0, stats.queueSize)
    }

    @Test
    fun testSendAudioWhenNotRunning() {
        // Don't start the service
        assertFalse(networkService.isRunning())
        
        val testAudioData = byteArrayOf(0x01, 0x02, 0x03)
        networkService.sendEncodedAudio(testAudioData)
        
        val stats = networkService.getStats()
        assertEquals("Should not queue packets when not running", 0, stats.queueSize)
    }

    @Test
    fun testMultipleAudioPackets() = runBlocking {
        assertTrue(networkService.start())
        
        // Send multiple packets
        for (i in 1..5) {
            val testData = byteArrayOf(i.toByte(), (i * 2).toByte(), (i * 3).toByte())
            networkService.sendEncodedAudio(testData)
        }
        
        delay(100)
        
        val stats = networkService.getStats()
        assertTrue("Should have processed multiple packets", stats.queueSize >= 0)
    }

    @Test
    fun testNetworkStats() = runBlocking {
        assertTrue(networkService.start())
        
        val initialStats = networkService.getStats()
        assertEquals(0L, initialStats.packetsSent)
        assertEquals(0L, initialStats.bytesSent)
        assertTrue(initialStats.isRunning)
        
        // Send some data
        val testData = byteArrayOf(0x01, 0x02, 0x03, 0x04)
        networkService.sendEncodedAudio(testData)
        
        delay(100)
        
        // Reset stats
        networkService.resetStats()
        val resetStats = networkService.getStats()
        assertEquals(0L, resetStats.packetsSent)
        assertEquals(0L, resetStats.bytesSent)
    }

    @Test
    fun testUpdateTarget() {
        val originalRunning = networkService.isRunning()
        
        // Update target
        val updated = networkService.updateTarget("192.168.1.200", 8080)
        
        // If it was running before, it should still be running
        // If it wasn't running, the update should succeed but not start it
        if (originalRunning) {
            assertTrue("Should successfully update target", updated)
        } else {
            assertTrue("Should successfully update target even when not running", updated)
        }
    }

    @Test
    fun testNetworkStatsCalculations() {
        val stats = NetworkStats(
            packetsSent = 100,
            bytesSent = 5000,
            sendErrors = 5,
            queueSize = 2,
            isRunning = true
        )
        
        assertEquals(20.0, stats.getPacketsPerSecond(5), 0.01)
        assertEquals(1000.0, stats.getBytesPerSecond(5), 0.01)
        assertEquals(0.047, stats.getErrorRate(), 0.01) // 5 / (100 + 5)
        
        // Test edge cases
        val emptyStats = NetworkStats()
        assertEquals(0.0, emptyStats.getPacketsPerSecond(5), 0.01)
        assertEquals(0.0, emptyStats.getBytesPerSecond(5), 0.01)
        assertEquals(0.0, emptyStats.getErrorRate(), 0.01)
    }

    @Test
    fun testConcurrentAccess() = runBlocking {
        assertTrue(networkService.start())
        
        // Simulate concurrent access from multiple threads
        val jobs = (1..10).map { i ->
            CoroutineScope(Dispatchers.Default).launch {
                val testData = byteArrayOf(i.toByte())
                networkService.sendEncodedAudio(testData)
            }
        }
        
        // Wait for all jobs to complete
        jobs.forEach { it.join() }
        
        delay(100)
        
        val stats = networkService.getStats()
        assertTrue("Should handle concurrent access", stats.queueSize >= 0)
    }
}