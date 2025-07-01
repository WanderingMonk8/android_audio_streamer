package com.example.audiocapture

import com.example.audiocapture.encoder.Encoder
import com.example.audiocapture.encoder.FFmpegEncoder

import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future

class EncodingService {
    private val sampleRate = 48000
    private val channels = 2
    private val bitrate = "64k"
    
    private var encoder: Encoder? = FFmpegEncoder(sampleRate, channels, bitrate)
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private var encodingTask: Future<ByteArray>? = null

    fun encodeFrame(pcmData: ByteArray, callback: (ByteArray?) -> Unit) {
        encodingTask?.cancel(true)
        encodingTask = executor.submit<ByteArray> {
            encoder?.encode(pcmData)
        }.also { future ->
            executor.submit {
                callback(future.get())
            }
        }
    }

    fun release() {
        encodingTask?.cancel(true)
        executor.shutdownNow()
        encoder?.destroy()
        encoder = null
    }
}