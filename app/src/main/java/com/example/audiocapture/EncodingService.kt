package com.example.audiocapture
import com.arthenica.ffmpegkit.FFmpegKit
import com.arthenica.ffmpegkit.FFmpegKit
import com.arthenica.ffmpegkit.ReturnCode
import com.arthenica.ffmpegkit.StreamInformation
import java.nio.ByteBuffer

class EncodingService {
    private val frameSize = 120 // 2.5ms @48kHz
    private val channels = 2 // stereo
    private val sampleRate = 48000
    private val bitrate = "64k"

    fun encodeFrame(pcmData: ByteArray): ByteArray? {
        return try {
            val command = "-f s16le -ar $sampleRate -ac $channels -i pipe:0 -c:a libopus -b:a $bitrate -f ogg pipe:1"
            val session = FFmpegKit.executeWithInput(command, pcmData)
            
            if (ReturnCode.isSuccess(session.returnCode)) {
                session.output
            } else {
                null
            }
        } catch (e: Exception) {
            null
        }
    }

    fun release() {
        // No cleanup needed for FFmpegKit
    }
}