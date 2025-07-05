package com.example.audiocapture

import android.app.Activity
import android.os.Bundle
import android.util.Log
import android.widget.*
import com.example.audiocapture.network.NetworkService
import com.example.audiocapture.network.UdpSender
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress

class MainActivity : Activity() {
    
    private lateinit var ipAddressInput: EditText
    private lateinit var portInput: EditText
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var statusText: TextView
    private lateinit var logText: TextView
    
    private var isStreaming = false
    private var streamingThread: Thread? = null
    private var udpSender: UdpSender? = null
    
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
            
            // Create UDP sender with WiFi MulticastLock
            addLog("Creating UdpSender with WiFi MulticastLock...")
            udpSender = UdpSender(this@MainActivity, ipAddress, port)
            
            if (!udpSender!!.start()) {
                addLog("Failed to start UdpSender")
                runOnUiThread {
                    showError("Failed to start UDP sender")
                    stopStreaming()
                }
                return
            }
            
            addLog("UdpSender started successfully with WiFi MulticastLock")
            
            runOnUiThread {
                statusText.text = "Streaming..."
                statusText.setTextColor(android.graphics.Color.GREEN)
            }
            
            var packetCount = 0
            
            // Send test packets every 100ms using our enhanced UdpSender
            while (isStreaming) {
                try {
                    packetCount++
                    
                    // Create test audio packet
                    val testData = "AudioPacket#$packetCount".toByteArray()
                    
                    // Create AudioPacket and send via UdpSender
                    val audioPacket = com.example.audiocapture.network.AudioPacket(
                        sequenceId = packetCount.toUInt(),
                        timestamp = System.nanoTime().toULong(),
                        payload = testData
                    )
                    
                    val success = udpSender!!.sendPacket(audioPacket)
                    if (success) {
                        addLog("Sent packet $packetCount (${testData.size} bytes) via WiFi MulticastLock")
                    } else {
                        addLog("Failed to send packet $packetCount")
                    }
                    
                    Thread.sleep(100) // Send every 100ms
                    
                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    addLog("Send error: ${e.message}")
                    runOnUiThread {
                        showError("Send failed: ${e.message}")
                        stopStreaming()
                    }
                    break
                }
            }
            
            udpSender?.stop()
            addLog("UdpSender stopped and WiFi MulticastLock released")
            
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
    
    override fun onDestroy() {
        super.onDestroy()
        stopStreaming()
    }
}