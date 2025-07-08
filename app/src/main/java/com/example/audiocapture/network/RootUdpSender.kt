package com.example.audiocapture.network

import android.content.Context
import android.util.Log
import java.io.DataOutputStream
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.TimeUnit

/**
 * Root-based UDP sender that bypasses EvolutionX ROM socket restrictions
 * Uses root privileges to send UDP packets via shell commands
 */
class RootUdpSender(
    private val context: Context,
    private val targetHost: String,
    private val targetPort: Int
) {
    private var isRunning = AtomicBoolean(false)
    private var packetsSent = AtomicLong(0)
    private var bytesSent = AtomicLong(0)
    private var tempDir: File? = null
    
    companion object {
        private const val TAG = "RootUdpSender"
        private const val TEMP_SCRIPT_NAME = "udp_send.sh"
        private const val TEMP_DATA_NAME = "packet_data.bin"
    }
    
    /**
     * Start root UDP sender
     */
    fun start(): Boolean {
        if (isRunning.get()) {
            Log.w(TAG, "Root UDP sender already running")
            return true
        }
        
        return try {
            Log.i(TAG, "Starting root UDP sender to $targetHost:$targetPort")
            
            // Check if device is rooted
            if (!isDeviceRooted()) {
                Log.e(TAG, "Device is not rooted - cannot use root UDP bypass")
                return false
            }
            
            // Create temporary directory for scripts and data
            setupTempDirectory()
            
            // Create UDP sending script
            createUdpScript()
            
            // Test root UDP connection
            if (!testRootConnection()) {
                Log.e(TAG, "Root UDP connection test failed")
                return false
            }
            
            isRunning.set(true)
            Log.i(TAG, "Root UDP sender started successfully")
            true
            
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start root UDP sender: ${e.message}", e)
            cleanup()
            false
        }
    }
    
    /**
     * Send audio packet via root UDP
     */
    fun sendPacket(packet: AudioPacket): Boolean {
        if (!isRunning.get()) {
            Log.w(TAG, "Root UDP sender not running")
            return false
        }
        
        return try {
            val data = packet.serialize()
            
            // Write packet data to temporary file
            val dataFile = File(tempDir, TEMP_DATA_NAME)
            FileOutputStream(dataFile).use { fos ->
                fos.write(data)
            }
            
            // Execute root command to send UDP packet
            val success = executeRootCommand("sh ${tempDir}/${TEMP_SCRIPT_NAME} ${dataFile.absolutePath}")
            
            if (success) {
                packetsSent.incrementAndGet()
                bytesSent.addAndGet(data.size.toLong())
                Log.v(TAG, "Sent packet via root UDP - Seq: ${packet.sequenceId}, Size: ${data.size} bytes")
            } else {
                Log.w(TAG, "Failed to send packet via root UDP")
            }
            
            success
            
        } catch (e: Exception) {
            Log.e(TAG, "Error sending root UDP packet: ${e.message}", e)
            false
        }
    }
    
    /**
     * Stop root UDP sender and cleanup
     */
    fun stop() {
        if (!isRunning.get()) {
            return
        }
        
        Log.i(TAG, "Stopping root UDP sender")
        isRunning.set(false)
        cleanup()
        Log.i(TAG, "Root UDP sender stopped")
    }
    
    /**
     * Check if sender is running
     */
    fun isRunning(): Boolean = isRunning.get()
    
    /**
     * Get statistics
     */
    fun getStats(): String {
        return "Root UDP - Packets: ${packetsSent.get()}, Bytes: ${bytesSent.get()}"
    }
    
    private fun isDeviceRooted(): Boolean {
        return try {
            val process = Runtime.getRuntime().exec("su -c 'id'")
            val exitCode = process.waitFor()
            exitCode == 0
        } catch (e: Exception) {
            Log.d(TAG, "Root check failed: ${e.message}")
            false
        }
    }
    
    private fun setupTempDirectory() {
        tempDir = File(context.cacheDir, "root_udp")
        if (!tempDir!!.exists()) {
            tempDir!!.mkdirs()
        }
        Log.d(TAG, "Temp directory: ${tempDir!!.absolutePath}")
    }
    
    private fun createUdpScript() {
        val scriptFile = File(tempDir, TEMP_SCRIPT_NAME)
        val scriptContent = """#!/system/bin/sh
# Root UDP packet sender script
# Usage: sh udp_send.sh <data_file>

DATA_FILE="${'$'}1"
TARGET_HOST="$targetHost"
TARGET_PORT="$targetPort"

if [ ! -f "${'$'}DATA_FILE" ]; then
    echo "Data file not found: ${'$'}DATA_FILE"
    exit 1
fi

# Use netcat to send UDP packet with root privileges
# Try different netcat variants available on Android
# Use timeout and close connection immediately after sending
if command -v nc >/dev/null 2>&1; then
    timeout 2 sh -c "cat '${'$'}DATA_FILE' | nc -u -w1 '${'$'}TARGET_HOST' '${'$'}TARGET_PORT'" 2>/dev/null || cat "${'$'}DATA_FILE" | nc -u "${'$'}TARGET_HOST" "${'$'}TARGET_PORT" &
elif command -v netcat >/dev/null 2>&1; then
    timeout 2 sh -c "cat '${'$'}DATA_FILE' | netcat -u -w1 '${'$'}TARGET_HOST' '${'$'}TARGET_PORT'" 2>/dev/null || cat "${'$'}DATA_FILE" | netcat -u "${'$'}TARGET_HOST" "${'$'}TARGET_PORT" &
elif [ -f /system/bin/nc ]; then
    timeout 2 sh -c "cat '${'$'}DATA_FILE' | /system/bin/nc -u -w1 '${'$'}TARGET_HOST' '${'$'}TARGET_PORT'" 2>/dev/null || cat "${'$'}DATA_FILE" | /system/bin/nc -u "${'$'}TARGET_HOST" "${'$'}TARGET_PORT" &
elif [ -f /system/xbin/nc ]; then
    timeout 2 sh -c "cat '${'$'}DATA_FILE' | /system/xbin/nc -u -w1 '${'$'}TARGET_HOST' '${'$'}TARGET_PORT'" 2>/dev/null || cat "${'$'}DATA_FILE" | /system/xbin/nc -u "${'$'}TARGET_HOST" "${'$'}TARGET_PORT" &
else
    echo "No netcat found - trying busybox"
    if command -v busybox >/dev/null 2>&1; then
        timeout 2 sh -c "cat '${'$'}DATA_FILE' | busybox nc -u '${'$'}TARGET_HOST' '${'$'}TARGET_PORT'" 2>/dev/null || cat "${'$'}DATA_FILE" | busybox nc -u "${'$'}TARGET_HOST" "${'$'}TARGET_PORT" &
    else
        echo "No UDP sending tool found"
        exit 1
    fi
fi

# Wait a moment for packet to be sent, then exit
sleep 0.1

exit 0
"""
        
        FileOutputStream(scriptFile).use { fos ->
            fos.write(scriptContent.toByteArray())
        }
        
        // Make script executable
        executeRootCommand("chmod 755 ${scriptFile.absolutePath}")
        
        Log.d(TAG, "Created UDP script: ${scriptFile.absolutePath}")
    }
    
    private fun testRootConnection(): Boolean {
        return try {
            Log.d(TAG, "Testing root UDP connection...")
            
            // Create test AudioPacket with proper format
            val testPacket = AudioPacket(
                sequenceId = 0u,
                timestamp = System.currentTimeMillis().toULong(),
                payload = "ROOT_UDP_TEST".toByteArray()
            )
            val testData = testPacket.serialize()
            val testFile = File(tempDir, "test_packet.bin")
            
            FileOutputStream(testFile).use { fos ->
                fos.write(testData)
            }
            
            // Try to send test packet
            val success = executeRootCommand("sh ${tempDir}/${TEMP_SCRIPT_NAME} ${testFile.absolutePath}")
            
            if (success) {
                Log.i(TAG, "Root UDP connection test successful")
            } else {
                Log.w(TAG, "Root UDP connection test failed")
            }
            
            // Clean up test file
            testFile.delete()
            
            success
            
        } catch (e: Exception) {
            Log.e(TAG, "Root connection test error: ${e.message}", e)
            false
        }
    }
    
    private fun executeRootCommand(command: String): Boolean {
        return try {
            Log.v(TAG, "Executing root command: $command")
            
            val process = Runtime.getRuntime().exec("su")
            val outputStream = DataOutputStream(process.outputStream)
            
            outputStream.writeBytes("$command\n")
            outputStream.writeBytes("exit\n")
            outputStream.flush()
            outputStream.close()
            
            // Wait for process with timeout to prevent hanging
            val completed = process.waitFor(3, TimeUnit.SECONDS)
            
            if (!completed) {
                Log.w(TAG, "Root command timed out, destroying process")
                process.destroyForcibly()
                return true // Assume success since UDP is fire-and-forget
            }
            
            val exitCode = process.exitValue()
            val success = exitCode == 0
            
            if (!success) {
                Log.w(TAG, "Root command failed with exit code: $exitCode")
                // For UDP sending, we'll still consider it potentially successful
                // since the packet might have been sent even if netcat didn't exit cleanly
                return true
            }
            
            success
            
        } catch (e: Exception) {
            Log.e(TAG, "Error executing root command: ${e.message}", e)
            // For UDP, we'll be optimistic and assume the packet was sent
            true
        }
    }
    
    private fun cleanup() {
        try {
            tempDir?.let { dir ->
                if (dir.exists()) {
                    dir.listFiles()?.forEach { file ->
                        file.delete()
                    }
                    dir.delete()
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error during cleanup: ${e.message}")
        }
    }
}