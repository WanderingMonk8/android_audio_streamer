package com.example.audiocapture

import com.example.audiocapture.network.AudioPacket
import org.junit.Test
import org.junit.Assert.*

/**
 * Simple packet format tests that don't require network connections
 */
class SimplePacketTest {

    @Test
    fun testBasicPacketCreation() {
        val testData = byteArrayOf(0x01, 0x02, 0x03, 0x04)
        val packet = AudioPacket(123u, 456789u, testData)
        
        assertEquals(123u, packet.sequenceId)
        assertEquals(456789u, packet.timestamp)
        assertEquals(4u, packet.payloadSize)
        assertArrayEquals(testData, packet.payload)
        assertTrue(packet.isValid())
    }

    @Test
    fun testPacketSerialization() {
        val testData = byteArrayOf(0xAA.toByte(), 0xBB.toByte())
        val packet = AudioPacket(0x12345678u, 0x123456789ABCDEF0u, testData)
        
        val serialized = packet.serialize()
        
        // Check total size: 4 + 8 + 4 + 2 = 18 bytes
        assertEquals(18, serialized.size)
        
        // Verify packet can be deserialized
        val deserialized = AudioPacket.deserialize(serialized)
        assertNotNull(deserialized)
        assertEquals(packet, deserialized)
    }

    @Test
    fun testPacketCompatibilityWithPCFormat() {
        // Test that our packet format matches PC receiver expectations
        val testPayload = byteArrayOf(0x01, 0x02, 0x03)
        val packet = AudioPacket(1u, 1000000u, testPayload)
        
        val serialized = packet.serialize()
        
        // Expected format: seq_id(4) + timestamp(8) + payload_size(4) + payload(3) = 19 bytes
        assertEquals(19, serialized.size)
        
        // Check little endian format
        assertEquals(0x01.toByte(), serialized[0])  // seq_id low byte
        assertEquals(0x00.toByte(), serialized[1])  // seq_id
        assertEquals(0x00.toByte(), serialized[2])  // seq_id  
        assertEquals(0x00.toByte(), serialized[3])  // seq_id high byte
        
        // Check payload size
        assertEquals(0x03.toByte(), serialized[12]) // payload_size low byte
        assertEquals(0x00.toByte(), serialized[13]) // payload_size
        assertEquals(0x00.toByte(), serialized[14]) // payload_size
        assertEquals(0x00.toByte(), serialized[15]) // payload_size high byte
        
        // Check payload
        assertEquals(0x01.toByte(), serialized[16])
        assertEquals(0x02.toByte(), serialized[17])
        assertEquals(0x03.toByte(), serialized[18])
    }

    @Test
    fun testEmptyPacket() {
        val emptyData = byteArrayOf()
        val packet = AudioPacket(100u, 200u, emptyData)
        
        assertTrue(packet.isValid())
        assertEquals(16, packet.totalSize()) // 4 + 8 + 4 + 0
        
        val serialized = packet.serialize()
        val deserialized = AudioPacket.deserialize(serialized)
        
        assertNotNull(deserialized)
        assertEquals(packet, deserialized)
    }
}