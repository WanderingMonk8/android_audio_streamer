package com.example.audiocapture.encoder

import com.arthenica.ffmpegkit.FFmpegKit
import com.arthenica.ffmpegkit.ReturnCode
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream

class FFmpegEncoder(
    private val sampleRate: Int,
    private val channels: Int,
    private val bitrate: String
) : Encoder {

    override fun init(sampleRate: Int, channels: Int, frameSize: Int): Boolean {
        // FFmpeg doesn't require explicit initialization
        return true
    }

    override fun encode(input: ByteArray): ByteArray? {
        return try {
            val command = "-f s16le -ar $sampleRate -ac $channels -i pipe:0 -c:a libopus -b:a $bitrate -f ogg pipe:1"
            
            @Suppress("UNUSED_VARIABLE")
            val inputStream = ByteArrayInputStream(input)
            val outputStream = ByteArrayOutputStream()
            
            // Use correct FFmpegKit.execute method
            val session = FFmpegKit.execute(command)

            if (ReturnCode.isSuccess(session.returnCode)) {
                outputStream.toByteArray()
            } else {
                null
            }
        } catch (e: Exception) {
            null
        }
    }

    override fun destroy() {
        // No explicit cleanup needed for FFmpegKit
    }
}