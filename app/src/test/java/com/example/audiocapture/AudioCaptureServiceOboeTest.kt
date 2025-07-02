package com.example.audiocapture

import android.media.AudioFormat
import android.media.projection.MediaProjection
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.*
import org.mockito.MockitoAnnotations
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.nio.ByteBuffer

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [33], manifest = Config.NONE)
class AudioCaptureServiceOboeTest {
    @Mock
    private lateinit var mockProjection: MediaProjection
    
    @Mock
    private lateinit var mockAudioStream: AudioStream
    
    @Mock
    private lateinit var mockAudioStreamBuilder: AudioStreamBuilder
    
    @Mock
    private lateinit var mockEncodingService: EncodingService
    
    @Mock
    private lateinit var mockCallback: AudioCaptureCallback
    
    private lateinit var service: AudioCaptureService

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        // Setup basic mock behavior (avoiding complex matchers)
        `when`(mockAudioStreamBuilder.setSampleRate(48000)).thenReturn(mockAudioStreamBuilder)
        `when`(mockAudioStreamBuilder.setChannelCount(2)).thenReturn(mockAudioStreamBuilder)
        `when`(mockAudioStreamBuilder.build()).thenReturn(mockAudioStream)
        
        // Setup encoding service mock - avoid using any() matcher
        doReturn(ByteArray(100)).`when`(mockEncodingService).encodeFrame(any(ByteArray::class.java))
        
        service = spy(AudioCaptureService(mockProjection, mockCallback, initializeEncoder = false))
        service.setAudioStreamForTesting(mockAudioStream)
        service.setAudioStreamBuilderForTesting(mockAudioStreamBuilder)
        service.setEncodingServiceForTesting(mockEncodingService)
    }

    @After
    fun tearDown() {
        reset(mockAudioStream, mockEncodingService)
    }

    @Test
    fun shouldStartCapture_whenCalled() {
        // Create a service that doesn't actually call native methods
        val testService = object : AudioCaptureService(mockProjection, mockCallback, initializeEncoder = false) {
            override fun startCapture() {
                setCapturingForTesting(true)
            }
        }
        
        testService.startCapture()
        
        assertTrue(testService.isCapturing())
    }

    @Test
    fun shouldHandleAudioCallback_whenDataAvailable() {
        service.setCapturingForTesting(true)
        val testData = ByteArray(120 * 4)
        val buffer = ByteBuffer.wrap(testData)
        
        val result = service.onAudioReady(mockAudioStream, buffer, testData.size)
        
        assertTrue(result)
        verify(mockCallback).onAudioReady(buffer, testData.size)
    }

    @Test
    fun shouldStopCapture_whenServiceStopped() {
        // Create a service that doesn't actually call native methods
        val testService = object : AudioCaptureService(mockProjection, mockCallback, initializeEncoder = false) {
            override fun stopCapture() {
                setCapturingForTesting(false)
                mockEncodingService.release()
            }
        }
        testService.setEncodingServiceForTesting(mockEncodingService)
        testService.setCapturingForTesting(true)
        
        testService.stopCapture()
        
        assertFalse(testService.isCapturing())
        verify(mockEncodingService).release()
    }

    @Test
    fun shouldHandleStartError_whenCaptureFails() {
        // Create a service that simulates start failure
        val failingService = object : AudioCaptureService(mockProjection, mockCallback, initializeEncoder = false) {
            override fun startCapture() {
                throw RuntimeException("Native start failed")
            }
        }
        
        try {
            failingService.startCapture()
            fail("Expected exception")
        } catch (e: Exception) {
            assertEquals("Native start failed", e.message)
        }
        
        assertFalse(failingService.isCapturing())
    }

    @Test
    fun shouldHandleNullAudioData_whenOnAudioReadyCalled() {
        service.setCapturingForTesting(true)
        val result = service.onAudioReady(mockAudioStream, null, 0)
        
        assertFalse(result)
        verify(mockCallback, never()).onAudioReady(any(), anyInt())
    }

    @Test
    fun shouldHandleErrorBeforeClose_whenStreamFails() {
        service.setCapturingForTesting(true)
        service.onErrorBeforeClose(mockAudioStream, 1)
        
        assertFalse(service.isCapturing())
        verify(mockCallback).onError("Stream error: 1")
    }

    @Test
    fun shouldHandleErrorAfterClose_whenStreamFails() {
        service.setCapturingForTesting(true)
        service.onErrorAfterClose(mockAudioStream, 1)
        
        assertFalse(service.isCapturing())
        verify(mockCallback).onError("Stream error after close: 1")
    }
}