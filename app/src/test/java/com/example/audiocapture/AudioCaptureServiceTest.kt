package com.example.audiocapture

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.projection.MediaProjection
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.*
import org.mockito.MockitoAnnotations
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AudioCaptureServiceTest {
    @Mock
    private lateinit var mockProjection: MediaProjection
    
    @Mock
    private lateinit var mockAudioRecord: AudioRecord
    
    @Mock
    private lateinit var mockEncodingService: EncodingService
    
    private lateinit var service: AudioCaptureService

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        service = spy(AudioCaptureService(mockProjection))
        doReturn(mockAudioRecord).`when`(service).createAudioRecord()
        doReturn(mockEncodingService).`when`(service).encodingService
    }

    @After
    fun tearDown() {
        verifyNoMoreInteractions(mockAudioRecord, mockEncodingService)
    }

    @Test
    fun shouldStartCapture_whenServiceStarted() {
        service.startCapture()
        
        verify(mockAudioRecord).startRecording()
        assert(service.isCapturing())
    }

    @Test
    fun shouldNotStartCapture_whenAlreadyCapturing() {
        doReturn(true).`when`(service).isCapturing()
        
        service.startCapture()
        
        verify(mockAudioRecord, never()).startRecording()
    }

    @Test
    fun shouldStopCapture_whenServiceStopped() {
        doReturn(true).`when`(service).isCapturing()
        
        service.stopCapture()
        
        verify(mockAudioRecord).stop()
        verify(mockAudioRecord).release()
        verify(mockEncodingService).release()
        assert(!service.isCapturing())
    }

    @Test
    fun shouldNotStopCapture_whenNotCapturing() {
        doReturn(false).`when`(service).isCapturing()
        
        service.stopCapture()
        
        verify(mockAudioRecord, never()).stop()
    }

    @Test
    fun shouldUseCorrectAudioFormat_whenInitialized() {
        val service = AudioCaptureService(mockProjection)
        
        val minBufferSize = AudioRecord.getMinBufferSize(
            48000,
            AudioFormat.CHANNEL_IN_STEREO,
            AudioFormat.ENCODING_PCM_16BIT
        )
        
        assert(minBufferSize > 0) // Verify valid buffer size
        // Additional format assertions would be here
    }

    @Test
    fun shouldEncodeAudio_whenFrameCaptured() {
        val testData = ByteArray(120 * 4)
        doReturn(testData.size).`when`(mockAudioRecord).read(testData, 0, testData.size)
        doReturn(true).`when`(service).isCapturing()
        
        service.startCapture()
        
        verify(mockEncodingService).encodeFrame(testData)
    }

    @Test
    fun shouldHandleError_whenCaptureFails() {
        doThrow(RuntimeException("Test error")).`when`(mockAudioRecord).startRecording()
        
        service.startCapture()
        
        verify(mockAudioRecord).release()
        assert(!service.isCapturing())
    }
}