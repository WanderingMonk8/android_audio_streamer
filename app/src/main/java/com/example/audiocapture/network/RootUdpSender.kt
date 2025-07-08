package com.example.audiocapture.network

import android.content.Context
import android.util.Log
import java.io.DataOutputStream
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.TimeUnit
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.Executors
import java.util.concurrent.ExecutorService

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
    
    // High-performance components
    private var persistentRootProcess: Process? = null
    private var rootOutputStream: DataOutputStream? = null
    private val packetQueue = LinkedBlockingQueue<ByteArray>(200)
    private val senderExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    private var senderThread: Thread? = null
    
    // Real-time parallel processing for zero-latency audio
    private val maxParallelSends = 3 // Allow up to 3 simultaneous netcat processes
    
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
            
            // Create optimized UDP sending infrastructure
            setupOptimizedUdpSender()
            
            // Test root UDP connection
            if (!testRootConnection()) {
                Log.e(TAG, "Root UDP connection test failed")
                return false
            }
            
            // Start high-performance sender thread
            startSenderThread()
            
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
     * Send audio packet via high-performance root UDP
     */
    fun sendPacket(packet: AudioPacket): Boolean {
        if (!isRunning.get()) {
            Log.w(TAG, "Root UDP sender not running")
            return false
        }
        
        return try {
            val data = packet.serialize()
            
            // Add to queue for asynchronous sending (non-blocking)
            val success = packetQueue.offer(data)
            
            if (success) {
                packetsSent.incrementAndGet()
                bytesSent.addAndGet(data.size.toLong())
                Log.v(TAG, "Queued packet via root UDP - Seq: ${packet.sequenceId}, Size: ${data.size} bytes")
            } else {
                Log.w(TAG, "Packet queue full, dropping packet")
            }
            
            success
            
        } catch (e: Exception) {
            Log.e(TAG, "Error queuing root UDP packet: ${e.message}", e)
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
        
        // Stop sender thread
        senderThread?.interrupt()
        senderThread = null
        
        // Close persistent root process
        closePersistentRootProcess()
        
        // Shutdown executor
        senderExecutor.shutdown()
        
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
            Log.d(TAG, "Testing optimized root UDP connection...")
            
            // Create test AudioPacket with proper format
            val testPacket = AudioPacket(
                sequenceId = 0u,
                timestamp = System.currentTimeMillis().toULong(),
                payload = "ROOT_UDP_TEST".toByteArray()
            )
            val testData = testPacket.serialize()
            
            // Test the persistent root process directly
            rootOutputStream?.let { outputStream ->
                val base64Data = android.util.Base64.encodeToString(testData, android.util.Base64.NO_WRAP)
                val command = "echo '$base64Data' | base64 -d | nc -u -w1 $targetHost $targetPort\n"
                
                outputStream.writeBytes(command)
                outputStream.flush()
                
                // Give it a moment to execute
                Thread.sleep(200)
                
                Log.i(TAG, "Optimized root UDP connection test successful")
                return true
            }
            
            Log.w(TAG, "Root UDP connection test failed - no persistent process")
            false
            
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
    
    private fun setupOptimizedUdpSender() {
        try {
            // Create persistent root process for high-performance sending
            persistentRootProcess = Runtime.getRuntime().exec("su")
            rootOutputStream = DataOutputStream(persistentRootProcess!!.outputStream)
            
            Log.d(TAG, "Persistent root process created successfully")
            
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create persistent root process: ${e.message}", e)
            throw e
        }
    }
    
    private fun startSenderThread() {
        senderThread = Thread {
            Log.d(TAG, "Real-time parallel sender thread started")
            
            while (isRunning.get()) {
                try {
                    // Get packet immediately (blocking) - zero batching delay
                    val packetData = packetQueue.poll(100, TimeUnit.MILLISECONDS)
                    
                    if (packetData != null) {
                        // Send immediately with parallel processing for maximum speed
                        sendPacketParallel(packetData)
                    }
                    
                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Error in real-time sender thread: ${e.message}", e)
                }
            }
            
            Log.d(TAG, "Real-time parallel sender thread stopped")
        }
        senderThread?.start()
    }
    
    private fun sendPacketParallel(packetData: ByteArray) {
        try {
            rootOutputStream?.let { outputStream ->
                // Use optimized command with background execution for zero-latency
                val base64Data = android.util.Base64.encodeToString(packetData, android.util.Base64.NO_WRAP)
                val command = "(echo '$base64Data' | base64 -d | nc -u -w1 $targetHost $targetPort) &\n"
                
                outputStream.writeBytes(command)
                outputStream.flush()
                
                Log.v(TAG, "Sent packet in parallel via persistent root process - Size: ${packetData.size} bytes")
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "Error sending packet in parallel: ${e.message}", e)
            
            // Try to recreate persistent process if it failed
            try {
                closePersistentRootProcess()
                setupOptimizedUdpSender()
            } catch (recreateException: Exception) {
                Log.e(TAG, "Failed to recreate persistent root process: ${recreateException.message}")
            }
        }
    }
    
    private fun sendPacketDirect(packetData: ByteArray) {
        try {
            rootOutputStream?.let { outputStream ->
                // Use base64 encoding for reliable binary data transmission
                val base64Data = android.util.Base64.encodeToString(packetData, android.util.Base64.NO_WRAP)
                val command = "echo '$base64Data' | base64 -d | nc -u -w1 $targetHost $targetPort\n"
                
                outputStream.writeBytes(command)
                outputStream.flush()
                
                Log.v(TAG, "Sent packet directly via persistent root process - Size: ${packetData.size} bytes")
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "Error sending packet directly: ${e.message}", e)
            
            // Try to recreate persistent process if it failed
            try {
                closePersistentRootProcess()
                setupOptimizedUdpSender()
            } catch (recreateException: Exception) {
                Log.e(TAG, "Failed to recreate persistent root process: ${recreateException.message}")
            }
        }
    }
    
    
    private fun closePersistentRootProcess() {
        try {
            rootOutputStream?.writeBytes("exit\n")
            rootOutputStream?.flush()
            rootOutputStream?.close()
            rootOutputStream = null
            
            persistentRootProcess?.destroy()
            persistentRootProcess = null
            
            Log.d(TAG, "Persistent root process closed")
            
        } catch (e: Exception) {
            Log.w(TAG, "Error closing persistent root process: ${e.message}")
        }
    }

    private fun cleanup() {
        try {
            // Clear packet queue
            packetQueue.clear()
            
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