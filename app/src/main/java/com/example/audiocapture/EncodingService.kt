package com.example.audiocapture

import com.btelman.opuscodec.opus.Opus
import com.btelman.opuscodec.opus.OpusEncoder
import java.nio.ByteBuffer

class EncodingService {
    private var encoder: OpusEncoder? = null
    private val frameSize = 120 // 2.5ms @48kHz
    private val channels = 2 // stereo

    fun initEncoder(): Boolean {
        return try {
            encoder = OpusEncoder().apply {
                init(48000, channels, Opus.OPUS_APPLICATION_AUDIO)
                setBitrate(64000) // 64kbps for low latency
                setComplexity(5) // Balanced quality vs CPU
            }
            true
        } catch (e: Exception) {
            false
        }
    }

    fun encodeFrame(pcmData: ByteArray): ByteArray? {
        if (encoder == null && !initEncoder()) {
            return null
        }

        return try {
            val outputBuffer = ByteBuffer.allocate(400) // Max size for Opus frame
            val encodedSize = encoder!!.encode(
                pcmData, 0, frameSize * channels * 2, // 16-bit samples
                outputBuffer.array(), 0, outputBuffer.capacity()
            )
            
            if (encodedSize > 0) {
                outputBuffer.array().copyOf(encodedSize)
            } else {
                null
            }
        } catch (e: Exception) {
            null
        }
    }

    fun release() {
        encoder?.destroy()
        encoder = null
    }
}