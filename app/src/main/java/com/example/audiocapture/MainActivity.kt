package com.example.audiocapture

import android.app.Activity
import android.os.Bundle
import android.util.Log
import android.widget.*
import com.example.audiocapture.network.NetworkService
import com.example.audiocapture.network.UdpSender
import com.example.audiocapture.network.NativeUdpSender
import com.example.audiocapture.network.SmartAudioStreamer
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.atomic.AtomicLong

class MainActivity : Activity() {
    
    private lateinit var ipAddressInput: EditText
    private lateinit var portInput: EditText
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var statusText: TextView
    private lateinit var logText: TextView
    
    private var isStreaming = false
    private var streamingThread: Thread? = null
    private var smartStreamer: SmartAudioStreamer? = null
    private var audioCaptureService: AudioCaptureService? = null
    private val audioQueue = LinkedBlockingQueue<ByteArray>(100) // Buffer for audio data
    private val packetSequence = AtomicLong(0)
    
    companion object {
        private const val TAG = "MainActivity"
    }
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        Log.i(TAG, "=== MainActivity onCreate START ===")
        System.out.println("AUDIOCAPTURE: MainActivity onCreate START")
        
        createUI()
        
        Log.i(TAG, "=== MainActivity onCreate SUCCESS ===")
        System.out.println("AUDIOCAPTURE: MainActivity onCreate SUCCESS")
    }
    
    private fun createUI() {
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(40, 40, 40, 40)
        }
        
        // Title
        val titleText = TextView(this).apply {
            text = "Audio Capture - UDP Streaming"
            textSize = 24f
            setPadding(0, 0, 0, 30)
            gravity = android.view.Gravity.CENTER
        }
        layout.addView(titleText)
        
        // IP Address Input
        val ipLabel = TextView(this).apply {
            text = "PC IP Address:"
            textSize = 16f
            setPadding(0, 0, 0, 8)
        }
        layout.addView(ipLabel)
        
        ipAddressInput = EditText(this).apply {
            setText("192.168.1.103") // Default IP
            hint = "Enter PC IP address (e.g., 192.168.1.103)"
            textSize = 16f
            setPadding(16, 16, 16, 16)
            inputType = android.text.InputType.TYPE_CLASS_TEXT
        }
        layout.addView(ipAddressInput)
        
        // Port Input
        val portLabel = TextView(this).apply {
            text = "Port:"
            textSize = 16f
            setPadding(0, 20, 0, 8)
        }
        layout.addView(portLabel)
        
        portInput = EditText(this).apply {
            setText("3000") // Default port
            hint = "Enter port (e.g., 3000, 5000, 8080)"
            textSize = 16f
            setPadding(16, 16, 16, 16)
            inputType = android.text.InputType.TYPE_CLASS_NUMBER
        }
        layout.addView(portInput)
        
        // Buttons
        val buttonLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, 30, 0, 20)
        }
        
        startButton = Button(this).apply {
            text = "Start Streaming"
            textSize = 16f
            setPadding(20, 20, 20, 20)
            setOnClickListener { startStreaming() }
        }
        buttonLayout.addView(startButton)
        
        stopButton = Button(this).apply {
            text = "Stop Streaming"
            textSize = 16f
            setPadding(20, 20, 20, 20)
            isEnabled = false
            setOnClickListener { stopStreaming() }
        }
        buttonLayout.addView(stopButton)
        
        layout.addView(buttonLayout)
        
        // Status
        statusText = TextView(this).apply {
            text = "Ready to stream"
            textSize = 18f
            setPadding(0, 0, 0, 20)
            setTextColor(android.graphics.Color.BLUE)
        }
        layout.addView(statusText)
        
        // Log output
        val logLabel = TextView(this).apply {
            text = "Log Output:"
            textSize = 16f
            setPadding(0, 0, 0, 8)
        }
        layout.addView(logLabel)
        
        logText = TextView(this).apply {
            text = "App started successfully\n"
            textSize = 12f
            setPadding(16, 16, 16, 16)
            setBackgroundColor(android.graphics.Color.LTGRAY)
            maxLines = 10
            isVerticalScrollBarEnabled = true
        }
        layout.addView(logText)
        
        setContentView(layout)
        
        // Add network info
        addLog("Network info loaded")
        checkNetworkInfo()
    }
    
    private fun startStreaming() {
        val ipAddress = ipAddressInput.text.toString().trim()
        val portText = portInput.text.toString().trim()
        
        if (ipAddress.isEmpty()) {
            showError("Please enter PC IP address")
            return
        }
        
        val port = try {
            portText.toInt()
        } catch (e: Exception) {
            showError("Please enter valid port number")
            return
        }
        
        if (port < 1 || port > 65535) {
            showError("Port must be between 1 and 65535")
            return
        }
        
        isStreaming = true
        updateUI()
        
        addLog("Starting UDP stream to $ipAddress:$port")
        statusText.text = "Connecting..."
        statusText.setTextColor(android.graphics.Color.rgb(255, 165, 0)) // Orange
        
        // Start streaming in background thread
        streamingThread = Thread {
            streamToPC(ipAddress, port)
        }
        streamingThread?.start()
    }
    
    private fun stopStreaming() {
        isStreaming = false
        streamingThread?.interrupt()
        streamingThread = null
        
        stopAudioCapture()
        
        updateUI()
        addLog("Streaming stopped")
        statusText.text = "Stopped"
        statusText.setTextColor(android.graphics.Color.RED)
    }
    
    private fun streamToPC(ipAddress: String, port: Int) {
        try {
            addLog("Resolving PC address: $ipAddress")
            val pcAddress = InetAddress.getByName(ipAddress)
            addLog("PC address resolved: ${pcAddress.hostAddress}")
            
            // Test reachability
            val reachable = pcAddress.isReachable(5000)
            addLog("PC reachable: $reachable")
            
            if (!reachable) {
                runOnUiThread {
                    showError("PC not reachable. Check IP address and network.")
                    stopStreaming()
                }
                return
            }
            
            // Use smart audio streamer with automatic protocol selection
            addLog("Starting smart audio streamer with automatic protocol selection...")
            smartStreamer = SmartAudioStreamer(this@MainActivity, ipAddress, port)
            
            if (!smartStreamer!!.start()) {
                addLog("All protocols failed (UDP, Native UDP, Root UDP)")
                runOnUiThread {
                    showError("Failed to start audio streaming (all protocols failed)")
                    stopStreaming()
                }
                return
            }
            
            val protocol = smartStreamer!!.getProtocolDescription()
            addLog("Smart streamer started successfully!")
            addLog("Active protocol: $protocol")
            
            runOnUiThread {
                statusText.text = "Streaming..."
                statusText.setTextColor(android.graphics.Color.GREEN)
            }
            
            // Start audio capture
            startAudioCapture()
            
            // Stream real audio data
            while (isStreaming) {
                try {
                    // Get audio data from queue (blocks until available)
                    val audioData = audioQueue.poll(50, java.util.concurrent.TimeUnit.MILLISECONDS)
                    
                    if (audioData != null) {
                        val sequenceId = packetSequence.incrementAndGet()
                        
                        // Create AudioPacket with real audio data
                        val audioPacket = com.example.audiocapture.network.AudioPacket(
                            sequenceId = sequenceId.toUInt(),
                            timestamp = System.nanoTime().toULong(),
                            payload = audioData
                        )
                        
                        val success = smartStreamer!!.sendPacket(audioPacket)
                        
                        if (success) {
                            val protocol = smartStreamer!!.getActiveProtocol()
                            if (sequenceId % 50 == 0L) { // Log every 50th packet to avoid spam
                                addLog("Sent audio packet $sequenceId (${audioData.size} bytes) via $protocol")
                            }
                        } else {
                            addLog("Failed to send audio packet $sequenceId")
                        }
                    }
                    
                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    addLog("Audio stream error: ${e.message}")
                    runOnUiThread {
                        showError("Audio streaming failed: ${e.message}")
                        stopStreaming()
                    }
                    break
                }
            }
            
            // Stop audio capture
            stopAudioCapture()
            
            smartStreamer?.stop()
            addLog("Smart audio streamer stopped")
            
        } catch (e: Exception) {
            addLog("Stream error: ${e.message}")
            runOnUiThread {
                showError("Connection failed: ${e.message}")
                stopStreaming()
            }
        }
    }
    
    private fun checkNetworkInfo() {
        try {
            val connectivityManager = getSystemService(android.content.Context.CONNECTIVITY_SERVICE) as android.net.ConnectivityManager
            val activeNetwork = connectivityManager.activeNetworkInfo
            
            val networkInfo = "Network: ${activeNetwork?.typeName} - ${activeNetwork?.isConnected}"
            val networkDetail = "State: ${activeNetwork?.detailedState}"
            
            addLog(networkInfo)
            addLog(networkDetail)
            
        } catch (e: Exception) {
            addLog("Network check failed: ${e.message}")
        }
    }
    
    private fun updateUI() {
        startButton.isEnabled = !isStreaming
        stopButton.isEnabled = isStreaming
        ipAddressInput.isEnabled = !isStreaming
        portInput.isEnabled = !isStreaming
    }
    
    private fun addLog(message: String) {
        runOnUiThread {
            val currentText = logText.text.toString()
            val lines = currentText.split("\n").toMutableList()
            
            // Keep only last 8 lines
            if (lines.size >= 8) {
                lines.removeAt(0)
            }
            
            lines.add(message)
            logText.text = lines.joinToString("\n")
            
            // Also log to system
            System.out.println("AUDIOCAPTURE: $message")
        }
    }
    
    private fun showError(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show()
        addLog("ERROR: $message")
    }
    
    private fun startAudioCapture() {
        try {
            addLog("Starting audio capture...")
            
            // Create a simple audio capture service for testing
            // We'll use the OboeWrapper directly for now
            val oboeWrapper = OboeWrapper()
            
            // Start a simple audio capture thread
            Thread {
                try {
                    oboeWrapper.nativeStartCapture(48000, 2)
                    addLog("Oboe audio capture started")
                    
                    while (isStreaming) {
                        try {
                            val buffer = oboeWrapper.getBuffer()
                            if (buffer != null && buffer.hasRemaining()) {
                                // Convert ByteBuffer to ByteArray
                                val audioBytes = ByteArray(buffer.remaining())
                                buffer.get(audioBytes)
                                
                                // Add to queue for streaming (non-blocking)
                                if (!audioQueue.offer(audioBytes)) {
                                    // Queue is full, remove oldest and add new
                                    audioQueue.poll()
                                    audioQueue.offer(audioBytes)
                                }
                            }
                            Thread.sleep(5) // 5ms polling interval
                        } catch (e: InterruptedException) {
                            break
                        } catch (e: Exception) {
                            addLog("Audio polling error: ${e.message}")
                        }
                    }
                    
                    oboeWrapper.nativeStopCapture()
                    addLog("Oboe audio capture stopped")
                    
                } catch (e: Exception) {
                    addLog("Oboe capture error: ${e.message}")
                }
            }.start()
            
            addLog("Audio capture thread started successfully")
            
        } catch (e: Exception) {
            addLog("Failed to start audio capture: ${e.message}")
            runOnUiThread {
                showError("Audio capture failed: ${e.message}")
                stopStreaming()
            }
        }
    }
    
    private fun stopAudioCapture() {
        try {
            audioCaptureService?.stopCapture()
            audioCaptureService = null
            audioQueue.clear()
            addLog("Audio capture stopped")
        } catch (e: Exception) {
            addLog("Error stopping audio capture: ${e.message}")
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        stopStreaming()
    }
}