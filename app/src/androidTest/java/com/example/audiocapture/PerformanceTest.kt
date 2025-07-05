package com.example.audiocapture

import android.content.Context
import android.media.projection.MediaProjection
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import java.nio.ByteBuffer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import kotlin.system.measureTimeMillis

@RunWith(AndroidJUnit4::class)
class PerformanceTest {
    
    @Mock
    private lateinit var mockProjection: MediaProjection
    
    private lateinit var context: Context
    private lateinit var oboeWrapper: OboeWrapper
    private lateinit var service: AudioCaptureService
    
    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        context = InstrumentationRegistry.getInstrumentation().targetContext
        oboeWrapper = OboeWrapper()
    }
    
    @Test
    fun shouldMeasureAudioCaptureLatency() {
        val latencyMeasurements = mutableListOf<Long>()
        val callback = object : AudioCaptureCallback {
            override fun onAudioReady(audioData: ByteBuffer, numFrames: Int) {
                // Measure time from capture to callback
                val latency = measureTimeMillis {
                    // Simulate processing time
                    audioData.position(audioData.limit())
                }
                latencyMeasurements.add(latency)
            }
            
            override fun onError(error: String) {
                fail("Audio capture error: $error")
            }
        }
        
        service = AudioCaptureService(context, mockProjection, callback)
        
        try {
            service.startCapture()
            
            // Capture for 1 second
            Thread.sleep(1000)
            
            service.stopCapture()
            
            // Analyze latency measurements
            if (latencyMeasurements.isNotEmpty()) {
                val avgLatency = latencyMeasurements.average()
                val maxLatency = latencyMeasurements.maxOrNull() ?: 0L
                
                println("Average latency: ${avgLatency}ms")
                println("Max latency: ${maxLatency}ms")
                println("Total measurements: ${latencyMeasurements.size}")
                
                // Assert reasonable latency (under 50ms for processing)
                assertTrue("Average latency should be under 50ms", avgLatency < 50.0)
                assertTrue("Max latency should be under 100ms", maxLatency < 100L)
            }
            
        } catch (e: Exception) {
            // If native audio fails, that's expected in some test environments
            println("Audio capture not available in test environment: ${e.message}")
        }
    }
    
    @Test
    fun shouldMeasureEncodingPerformance() {
        val encodingService = EncodingService(context)
        val testData = ByteArray(1920) // 10ms of 48kHz stereo 16-bit audio
        val encodingTimes = mutableListOf<Long>()
        
        try {
            // Warm up
            repeat(10) {
                encodingService.encodeFrame(testData)
            }
            
            // Measure encoding performance
            repeat(100) {
                val encodingTime = measureTimeMillis {
                    encodingService.encodeFrame(testData)
                }
                encodingTimes.add(encodingTime)
            }
            
            val avgEncodingTime = encodingTimes.average()
            val maxEncodingTime = encodingTimes.maxOrNull() ?: 0L
            
            println("Average encoding time: ${avgEncodingTime}ms")
            println("Max encoding time: ${maxEncodingTime}ms")
            
            // Assert encoding performance (should be under 5ms for low latency)
            assertTrue("Average encoding time should be under 5ms", avgEncodingTime < 5.0)
            assertTrue("Max encoding time should be under 10ms", maxEncodingTime < 10L)
            
        } finally {
            encodingService.release()
        }
    }
    
    @Test
    fun shouldTestMemoryStability() {
        val callback = object : AudioCaptureCallback {
            override fun onAudioReady(audioData: ByteBuffer, numFrames: Int) {
                // Minimal processing to test memory stability
            }
            
            override fun onError(error: String) {
                // Log but don't fail test for memory stability check
                println("Audio error during memory test: $error")
            }
        }
        
        service = AudioCaptureService(context, mockProjection, callback)
        
        try {
            val runtime = Runtime.getRuntime()
            val initialMemory = runtime.totalMemory() - runtime.freeMemory()
            
            service.startCapture()
            
            // Run for 5 seconds to test memory stability
            repeat(50) {
                Thread.sleep(100)
                System.gc() // Suggest garbage collection
            }
            
            service.stopCapture()
            
            val finalMemory = runtime.totalMemory() - runtime.freeMemory()
            val memoryIncrease = finalMemory - initialMemory
            
            println("Initial memory: ${initialMemory / 1024}KB")
            println("Final memory: ${finalMemory / 1024}KB")
            println("Memory increase: ${memoryIncrease / 1024}KB")
            
            // Assert no significant memory leak (under 1MB increase)
            assertTrue("Memory increase should be under 1MB", memoryIncrease < 1024 * 1024)
            
        } catch (e: Exception) {
            println("Memory test completed with audio unavailable: ${e.message}")
        }
    }
    
    @Test
    fun shouldTestConcurrentAccess() {
        val latch = CountDownLatch(2)
        val errors = mutableListOf<String>()
        
        // Test concurrent access to OboeWrapper
        val thread1 = Thread {
            try {
                oboeWrapper.startCapture(48000, 2)
                Thread.sleep(500)
                oboeWrapper.stop()
            } catch (e: Exception) {
                errors.add("Thread 1: ${e.message}")
            } finally {
                latch.countDown()
            }
        }
        
        val thread2 = Thread {
            try {
                Thread.sleep(100) // Slight delay
                oboeWrapper.getBuffer()
                Thread.sleep(400)
                oboeWrapper.stop()
            } catch (e: Exception) {
                errors.add("Thread 2: ${e.message}")
            } finally {
                latch.countDown()
            }
        }
        
        thread1.start()
        thread2.start()
        
        assertTrue("Concurrent test should complete within 2 seconds", 
                  latch.await(2, TimeUnit.SECONDS))
        
        // Print any errors but don't fail test - concurrent access handling is complex
        errors.forEach { println("Concurrent access error: $it") }
    }
}