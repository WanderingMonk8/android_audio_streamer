package com.example.audiocapture

import android.media.AudioFormat
import android.media.projection.MediaProjection
import android.util.Log
import com.google.oboe.AudioStream
import com.google.oboe.AudioStreamBuilder
import com.google.oboe.AudioStreamCallback
import java.util.concurrent.atomic.AtomicBoolean

class AudioCaptureService(private val mediaProjection: MediaProjection) : AudioStreamCallback() {
    internal var audioStream: AudioStream? = null
    internal var audioStreamBuilder: AudioStreamBuilder? = null
    internal val isCapturing = AtomicBoolean(false)
    internal val encodingService = EncodingService()

    fun startCapture() {
        if (isCapturing.get()) return

        try {
            audioStreamBuilder = AudioStreamBuilder()
                .setSampleRate(48000)
                .setChannelCount(2)
                .setFormat(AudioFormat.ENCODING_PCM_16BIT)
                .setPerformanceMode(AudioStreamBuilder.PERFORMANCE_MODE_LOW_LATENCY)
                .setSharingMode(AudioStreamBuilder.SHARING_MODE_EXCLUSIVE)
                .setCallback(this)
                .setBufferSize(16)

            audioStream = audioStreamBuilder?.build()
            audioStream?.start()
            isCapturing.set(true)
        } catch (e: Exception) {
            Log.e("AudioCapture", "Failed to start Oboe audio capture", e)
            stopCapture()
        }
    }

    fun stopCapture() {
        if (!isCapturing.getAndSet(false)) return

        try {
            audioStream?.stop()
            audioStream?.close()
            encodingService.release()
        } catch (e: Exception) {
            Log.e("AudioCapture", "Error stopping Oboe capture", e)
        } finally {
            audioStream = null
            audioStreamBuilder = null
        }
    }

    override fun onAudioReady(stream: AudioStream, audioData: ByteArray?, numFrames: Int): Boolean {
        if (!isCapturing.get() || audioData == null) return false

        encodingService.encodeFrame(audioData) { encodedPacket ->
            if (encodedPacket != null) {
                Log.d("AudioCapture", "Encoded ${encodedPacket.size} bytes")
                // TODO: Send encoded packet over network
            } else {
                Log.e("AudioCapture", "Encoding failed for frame")
            }
        }
        return true
    }

    override fun onErrorBeforeClose(stream: AudioStream, error: Int) {
        Log.e("AudioCapture", "Oboe stream error before close: $error")
        stopCapture()
    }

    override fun onErrorAfterClose(stream: AudioStream, error: Int) {
        Log.e("AudioCapture", "Oboe stream error after close: $error")
        stopCapture()
    }
}