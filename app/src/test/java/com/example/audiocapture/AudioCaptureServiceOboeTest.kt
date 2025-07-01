package com.example.audiocapture

import android.media.AudioFormat
import com.google.oboe.AudioStream
import com.google.oboe.AudioStreamBuilder
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.*
import org.mockito.MockitoAnnotations
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AudioCaptureServiceOboeTest {
    @Mock
    private lateinit var mockProjection: MediaProjection
    
    @Mock
    private lateinit var mockAudioStream: AudioStream
    
    @Mock
    private lateinit var mockAudioStreamBuilder: AudioStreamBuilder
    
    @Mock
    private lateinit var mockEncodingService: EncodingService
    
    private lateinit var service: AudioCaptureService

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        `when`(mockAudioStreamBuilder.setSampleRate(48000)).thenReturn(mockAudioStreamBuilder)
        `when`(mockAudioStreamBuilder.setChannelCount(2)).thenReturn(mockAudioStreamBuilder)
        `when`(mockAudioStreamBuilder.setFormat(AudioFormat.ENCODING_PCM_16BIT)).thenReturn(mockAudioStreamBuilder)
        `when`(mockAudioStreamBuilder.setPerformanceMode(anyInt())).thenReturn(mockAudioStreamBuilder)
        `when`(mockAudioStreamBuilder.setSharingMode(anyInt())).thenReturn(mockAudioStreamBuilder)
        `when`(mockAudioStreamBuilder.build()).thenReturn(mockAudioStream)
        
        service = spy(AudioCaptureService(mockProjection))
        service.audioStream = mockAudioStream
        service.audioStreamBuilder = mockAudioStreamBuilder
        service.encodingService = mockEncodingService
    }

    @After
    fun tearDown() {
        verifyNoMoreInteractions(mockAudioStream, mockEncodingService)
    }

    @Test
    fun shouldCreateOboeStreamWithCorrectParams_whenStarted() {
        service.startCapture()
        
        verify(mockAudioStreamBuilder).setSampleRate(48000)
        verify(mockAudioStreamBuilder).setChannelCount(2)
        verify(mockAudioStreamBuilder).setFormat(AudioFormat.ENCODING_PCM_16BIT)
        verify(mockAudioStreamBuilder).build()
        verify(mockAudioStream).start()
    }

    @Test
    fun shouldHandleOboeCallback_whenDataAvailable() {
        val testData = ByteArray(120 * 4)
        service.isCapturing.set(true)
        
        service.onAudioReady(mockAudioStream, testData, testData.size)
        
        verify(mockEncodingService).encodeFrame(testData)
    }

    @Test
    fun shouldStopOboeStream_whenServiceStopped() {
        service.isCapturing.set(true)
        
        service.stopCapture()
        
        verify(mockAudioStream).stop()
        verify(mockAudioStream).close()
        verify(mockEncodingService).release()
    }

    @Test
    fun shouldHandleOboeError_whenStreamFails() {
        `when`(mockAudioStreamBuilder.build()).thenThrow(RuntimeException("Oboe error"))
        
        service.startCapture()
        
        assertFalse(service.isCapturing.get())
    }
}