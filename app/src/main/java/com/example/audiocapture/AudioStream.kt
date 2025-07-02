package com.example.audiocapture

/**
 * Interface for audio stream abstraction to support testing
 */
interface AudioStream {
    fun start()
    fun stop()
    fun close()
    fun getState(): Int
    fun getSampleRate(): Int
    fun getChannelCount(): Int
}

/**
 * Interface for audio stream builder abstraction to support testing
 */
interface AudioStreamBuilder {
    fun setSampleRate(sampleRate: Int): AudioStreamBuilder
    fun setChannelCount(channelCount: Int): AudioStreamBuilder
    fun setFormat(format: Int): AudioStreamBuilder
    fun setPerformanceMode(mode: Int): AudioStreamBuilder
    fun setSharingMode(mode: Int): AudioStreamBuilder
    fun build(): AudioStream
}