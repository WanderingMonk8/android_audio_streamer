package com.example.audiocapture.encoder

interface Encoder {
    fun init(sampleRate: Int, channels: Int, frameSize: Int): Boolean
    fun encode(input: ByteArray): ByteArray?
    fun destroy()
}