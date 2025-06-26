package com.example.audiocapture

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.projection.MediaProjection
import android.util.Log
import java.util.concurrent.atomic.AtomicBoolean

class AudioCaptureService(private val mediaProjection: MediaProjection) {
    private var audioRecord: AudioRecord? = null
    private val isCapturing = AtomicBoolean(false)
    private var captureThread: Thread? = null

    fun startCapture() {
        if (isCapturing.get()) return

        val minBufferSize = AudioRecord.getMinBufferSize(
            48000,
            AudioFormat.CHANNEL_IN_STEREO,
            AudioFormat.ENCODING_PCM_16BIT
        )

        try {
            audioRecord = AudioRecord.Builder()
                .setAudioSource(MediaProjection.AUDIO_SOURCE)
                .setAudioFormat(AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(48000)
                    .setChannelMask(AudioFormat.CHANNEL_IN_STEREO)
                    .build())
                .setBufferSizeInBytes(minBufferSize * 2)
                .build()

            audioRecord?.startRecording()
            isCapturing.set(true)
            captureThread = Thread(Runnable {
                captureAudio()
            }, "AudioCaptureThread").apply {
                priority = Thread.MAX_PRIORITY
                start()
            }
        } catch (e: Exception) {
            Log.e("AudioCapture", "Failed to start audio capture", e)
            stopCapture()
        }
    }

    fun stopCapture() {
        if (!isCapturing.getAndSet(false)) return

        try {
            audioRecord?.stop()
            audioRecord?.release()
            captureThread?.join(1000)
        } catch (e: Exception) {
            Log.e("AudioCapture", "Error stopping capture", e)
        } finally {
            audioRecord = null
            captureThread = null
        }
    }

    private val encodingService = EncodingService()
    
    private fun captureAudio() {
        val buffer = ByteArray(120 * 4) // 2.5ms frame @48kHz stereo 16-bit
        while (isCapturing.get()) {
            try {
                val bytesRead = audioRecord?.read(buffer, 0, buffer.size) ?: 0
                if (bytesRead > 0) {
                    val encodedPacket = encodingService.encodeFrame(buffer)
                    if (encodedPacket != null) {
                        Log.d("AudioCapture", "Encoded ${encodedPacket.size} bytes")
                        // TODO: Send encoded packet over network
                    } else {
                        Log.e("AudioCapture", "Encoding failed for frame")
                    }
                }
            } catch (e: Exception) {
                Log.e("AudioCapture", "Error during capture", e)
                stopCapture()
            }
        }
        encodingService.release()
    }
}