package com.example.audiocapture

import com.example.audiocapture.encoder.Encoder
import com.example.audiocapture.encoder.FFmpegEncoder

import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.RejectedExecutionException

class EncodingService {
    private val sampleRate = 48000
    private val channels = 2
    private val bitrate = "128k"
    
    private var encoder: Encoder? = FFmpegEncoder(sampleRate, channels, bitrate,
        "-application lowdelay", "-frame_duration 2.5")
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private var encodingTask: Future<ByteArray>? = null

    fun encodeFrame(pcmData: ByteArray, callback: (ByteArray?) -> Unit) {
        if (pcmData.isEmpty() || encoder == null) {
            callback(null)
            return
        }

        try {
            encodingTask?.cancel(true)
            encodingTask = executor.submit<ByteArray> {
                try {
                    encoder?.encode(pcmData)
                } catch (e: Exception) {
                    null
                }
            }.also { future ->
                executor.submit {
                    try {
                        callback(future.get())
                    } catch (e: Exception) {
                        callback(null)
                    }
                }
            }
        } catch (e: RejectedExecutionException) {
            callback(null)
        }
    }

    fun release() {
        encodingTask?.cancel(true)
        executor.shutdownNow()
        try {
            encoder?.release()
        } catch (e: Exception) {
            // Ignore release errors
        }
        encoder = null
    }
}