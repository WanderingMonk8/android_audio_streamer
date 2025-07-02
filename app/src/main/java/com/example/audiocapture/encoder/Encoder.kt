package com.example.audiocapture.encoder

interface Encoder {
    fun init(sampleRate: Int, channels: Int, frameSize: Int): Boolean
    fun encode(input: ByteArray): ByteArray?
    fun encodeFrame(input: ByteArray): ByteArray? = encode(input)
    fun destroy()
    fun release() = destroy()
}