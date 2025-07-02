package com.example.audiocapture.network

import org.junit.Test
import org.junit.Assert.*

class AudioPacketTest {

    @Test
    fun testPacketConstruction() {
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
        val testData = byteArrayOf(0xAA.toByte(), 0xBB.toByte(), 0xCC.toByte(), 0xDD.toByte())
        val packet = AudioPacket(0x12345678u, 0x123456789ABCDEF0u, testData)
        
        val serialized = packet.serialize()
        
        // Check total size: 4 + 8 + 4 + 4 = 20 bytes
        assertEquals(20, serialized.size)
        assertEquals(20, packet.totalSize())
        
        // Check sequence ID (little endian)
        assertEquals(0x78.toByte(), serialized[0])
        assertEquals(0x56.toByte(), serialized[1])
        assertEquals(0x34.toByte(), serialized[2])
        assertEquals(0x12.toByte(), serialized[3])
        
        // Check payload size (little endian)
        assertEquals(0x04.toByte(), serialized[12])
        assertEquals(0x00.toByte(), serialized[13])
        assertEquals(0x00.toByte(), serialized[14])
        assertEquals(0x00.toByte(), serialized[15])
        
        // Check payload
        assertEquals(0xAA.toByte(), serialized[16])
        assertEquals(0xBB.toByte(), serialized[17])
        assertEquals(0xCC.toByte(), serialized[18])
        assertEquals(0xDD.toByte(), serialized[19])
    }

    @Test
    fun testPacketDeserialization() {
        val testData = byteArrayOf(0x11, 0x22, 0x33)
        val original = AudioPacket(0x87654321u, 0xFEDCBA9876543210u, testData)
        
        // Serialize and deserialize
        val serialized = original.serialize()
        val deserialized = AudioPacket.deserialize(serialized)
        
        assertNotNull(deserialized)
        assertEquals(0x87654321u, deserialized!!.sequenceId)
        assertEquals(0xFEDCBA9876543210u, deserialized.timestamp)
        assertEquals(3u, deserialized.payloadSize)
        assertArrayEquals(testData, deserialized.payload)
        assertTrue(deserialized.isValid())
    }

    @Test
    fun testPacketInvalidData() {
        // Test with insufficient data
        val invalidData = byteArrayOf(0x01, 0x02, 0x03)
        val packet = AudioPacket.deserialize(invalidData)
        assertNull(packet)
        
        // Test with mismatched payload size
        val mismatchedData = byteArrayOf(
            0x01, 0x00, 0x00, 0x00,  // sequence_id = 1
            0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timestamp = 2
            0x10, 0x00, 0x00, 0x00,  // payload_size = 16 (but only 2 bytes follow)
            0xAA.toByte(), 0xBB.toByte()
        )
        val packet2 = AudioPacket.deserialize(mismatchedData)
        assertNull(packet2)
    }

    @Test
    fun testEmptyPacket() {
        val emptyData = byteArrayOf()
        val packet = AudioPacket(100u, 200u, emptyData)
        
        assertEquals(100u, packet.sequenceId)
        assertEquals(200u, packet.timestamp)
        assertEquals(0u, packet.payloadSize)
        assertTrue(packet.payload.isEmpty())
        assertTrue(packet.isValid())
        assertEquals(16, packet.totalSize()) // 4 + 8 + 4 + 0
        
        // Test serialization/deserialization of empty packet
        val serialized = packet.serialize()
        assertEquals(16, serialized.size)
        
        val deserialized = AudioPacket.deserialize(serialized)
        assertNotNull(deserialized)
        assertTrue(deserialized!!.payload.isEmpty())
        assertTrue(deserialized.isValid())
    }

    @Test
    fun testPacketEquality() {
        val testData1 = byteArrayOf(0x01, 0x02, 0x03)
        val testData2 = byteArrayOf(0x01, 0x02, 0x03)
        val testData3 = byteArrayOf(0x04, 0x05, 0x06)
        
        val packet1 = AudioPacket(123u, 456u, testData1)
        val packet2 = AudioPacket(123u, 456u, testData2)
        val packet3 = AudioPacket(123u, 456u, testData3)
        val packet4 = AudioPacket(124u, 456u, testData1)
        
        assertEquals(packet1, packet2)
        assertNotEquals(packet1, packet3)
        assertNotEquals(packet1, packet4)
        
        assertEquals(packet1.hashCode(), packet2.hashCode())
    }

    @Test
    fun testLargePacket() {
        // Test with maximum reasonable payload size
        val largeData = ByteArray(65536) { (it % 256).toByte() }
        val packet = AudioPacket(999u, 123456789u, largeData)
        
        assertTrue(packet.isValid())
        assertEquals(65536u, packet.payloadSize)
        assertEquals(65552, packet.totalSize()) // 16 + 65536
        
        // Test serialization/deserialization
        val serialized = packet.serialize()
        val deserialized = AudioPacket.deserialize(serialized)
        
        assertNotNull(deserialized)
        assertEquals(packet, deserialized)
    }

    @Test
    fun testInvalidLargePacket() {
        // Test with oversized payload
        val oversizedData = ByteArray(65537) { 0x00 }
        val packet = AudioPacket(1u, 1u, oversizedData)
        
        assertFalse(packet.isValid())
    }
}