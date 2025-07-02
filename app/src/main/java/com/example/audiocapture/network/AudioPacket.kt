package com.example.audiocapture.network

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Audio packet structure for UDP transmission
 * Format: [sequence_id(4)] [timestamp(8)] [payload_size(4)] [payload(variable)]
 */
data class AudioPacket(
    val sequenceId: UInt,
    val timestamp: ULong,
    val payload: ByteArray
) {
    val payloadSize: UInt get() = payload.size.toUInt()
    
    /**
     * Serialize packet to bytes for transmission
     */
    fun serialize(): ByteArray {
        val totalSize = 16 + payload.size // 4 + 8 + 4 + payload_size
        val buffer = ByteBuffer.allocate(totalSize).order(ByteOrder.LITTLE_ENDIAN)
        
        buffer.putInt(sequenceId.toInt())
        buffer.putLong(timestamp.toLong())
        buffer.putInt(payloadSize.toInt())
        buffer.put(payload)
        
        return buffer.array()
    }
    
    /**
     * Get total packet size in bytes
     */
    fun totalSize(): Int = 16 + payload.size
    
    /**
     * Validate packet integrity
     */
    fun isValid(): Boolean = payloadSize.toInt() == payload.size && payloadSize <= 65536u
    
    companion object {
        /**
         * Deserialize bytes to packet
         */
        fun deserialize(data: ByteArray): AudioPacket? {
            if (data.size < 16) return null
            
            val buffer = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
            
            val sequenceId = buffer.int.toUInt()
            val timestamp = buffer.long.toULong()
            val payloadSize = buffer.int.toUInt()
            
            if (data.size < 16 + payloadSize.toInt()) return null
            
            val payload = ByteArray(payloadSize.toInt())
            buffer.get(payload)
            
            val packet = AudioPacket(sequenceId, timestamp, payload)
            return if (packet.isValid()) packet else null
        }
    }
    
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        
        other as AudioPacket
        
        if (sequenceId != other.sequenceId) return false
        if (timestamp != other.timestamp) return false
        if (!payload.contentEquals(other.payload)) return false
        
        return true
    }
    
    override fun hashCode(): Int {
        var result = sequenceId.hashCode()
        result = 31 * result + timestamp.hashCode()
        result = 31 * result + payload.contentHashCode()
        return result
    }
}