package com.example.audiocapture.network

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiManager
import android.os.Build
import android.util.Log
import kotlinx.coroutines.*
import java.net.*
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

/**
 * UDP sender for audio packets
 * Implements low-latency packet transmission to PC receiver
 */
class UdpSender(
    private val context: Context,
    private val targetHost: String,
    private val targetPort: Int
) {
    private var socket: DatagramSocket? = null
    private var targetAddress: InetSocketAddress? = null
    private var multicastLock: WifiManager.MulticastLock? = null
    private val isRunning = AtomicBoolean(false)
    
    // Statistics
    private val packetsSent = AtomicLong(0)
    private val bytesSent = AtomicLong(0)
    private val sendErrors = AtomicLong(0)
    
    // Coroutine scope for async operations
    private val senderScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    
    companion object {
        private const val TAG = "UdpSender"
        private const val SOCKET_TIMEOUT_MS = 5000
        private const val MAX_PACKET_SIZE = 2048
    }
    
    /**
     * Start the UDP sender
     */
    fun start(): Boolean {
        if (isRunning.get()) {
            Log.w(TAG, "UDP sender already running")
            return false
        }
        
        return try {
            // Check network permissions and state first
            Log.i(TAG, "Checking network permissions and state...")
            
            // Check if we have INTERNET permission
            val internetPermission = context.checkSelfPermission(android.Manifest.permission.INTERNET)
            Log.i(TAG, "INTERNET permission: $internetPermission")
            
            // Check connectivity using both old and new APIs
            val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
            val activeNetwork = connectivityManager?.activeNetworkInfo
            Log.i(TAG, "Active network (legacy): ${activeNetwork?.typeName}, connected: ${activeNetwork?.isConnected}")
            
            // Use newer Network API (Android 6.0+)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                val currentNetwork = connectivityManager?.activeNetwork
                val networkCapabilities = connectivityManager?.getNetworkCapabilities(currentNetwork)
                val hasInternet = networkCapabilities?.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) ?: false
                val hasWifi = networkCapabilities?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ?: false
                Log.i(TAG, "Network capabilities: hasInternet=$hasInternet, hasWifi=$hasWifi")
                
                // Try to bind process to WiFi network if available
                if (hasWifi && currentNetwork != null) {
                    try {
                        Log.i(TAG, "Attempting to bind process to WiFi network...")
                        connectivityManager.bindProcessToNetwork(currentNetwork)
                        Log.i(TAG, "Process bound to WiFi network successfully")
                    } catch (e: Exception) {
                        Log.w(TAG, "Failed to bind to WiFi network", e)
                    }
                }
            }
            
            // Acquire WiFi MulticastLock to prevent Android from filtering UDP packets
            Log.i(TAG, "Attempting to acquire WiFi MulticastLock...")
            val wifiManager = context.getSystemService(Context.WIFI_SERVICE) as? WifiManager
            if (wifiManager != null) {
                Log.i(TAG, "WifiManager obtained successfully")
                try {
                    multicastLock = wifiManager.createMulticastLock("AudioCaptureUDP").apply {
                        Log.i(TAG, "MulticastLock created with tag: AudioCaptureUDP")
                        setReferenceCounted(false)
                        Log.i(TAG, "MulticastLock reference counting disabled")
                        acquire()
                        Log.i(TAG, "MulticastLock.acquire() called")
                    }
                    
                    // Verify lock is held
                    val isHeld = multicastLock?.isHeld ?: false
                    Log.i(TAG, "WiFi MulticastLock acquired successfully - isHeld: $isHeld")
                    
                    // Log WiFi state
                    val wifiInfo = wifiManager.connectionInfo
                    Log.i(TAG, "WiFi Info - SSID: ${wifiInfo?.ssid}, IP: ${wifiInfo?.ipAddress}")
                    
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to acquire WiFi MulticastLock", e)
                    multicastLock = null
                }
            } else {
                Log.e(TAG, "WifiManager not available - this will likely cause UDP failures!")
            }
            
            // Resolve target address
            Log.i(TAG, "Resolving target address: $targetHost:$targetPort")
            targetAddress = InetSocketAddress(targetHost, targetPort)
            Log.i(TAG, "Target address resolved: ${targetAddress?.address?.hostAddress}:${targetAddress?.port}")
            
            // Create UDP socket - try multiple approaches to bypass Android restrictions
            Log.i(TAG, "Creating UDP socket...")
            Log.w(TAG, "Android reports no connectivity, but network apps work - bypassing validation...")
            
            var socketCreated = false
            var lastException: Exception? = null
            
            // Approach 1: Force network binding first, then create socket
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                try {
                    Log.i(TAG, "Approach 1: Trying to force network binding...")
                    val allNetworks = connectivityManager?.allNetworks
                    Log.i(TAG, "Found ${allNetworks?.size ?: 0} networks")
                    
                    allNetworks?.forEach { network ->
                        val caps = connectivityManager?.getNetworkCapabilities(network)
                        val hasWifi = caps?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ?: false
                        Log.i(TAG, "Network: $network, hasWifi: $hasWifi")
                        
                        if (hasWifi) {
                            try {
                                Log.i(TAG, "Binding to WiFi network: $network")
                                connectivityManager?.bindProcessToNetwork(network)
                                
                                // Now try to create socket
                                socket = DatagramSocket().apply {
                                    soTimeout = SOCKET_TIMEOUT_MS
                                    trafficClass = 0x10
                                    sendBufferSize = 64 * 1024
                                }
                                socketCreated = true
                                Log.i(TAG, "SUCCESS: Socket created after binding to network!")
                                return@forEach
                            } catch (e: Exception) {
                                Log.w(TAG, "Failed with network $network", e)
                                lastException = e
                            }
                        }
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "Network binding approach failed", e)
                    lastException = e
                }
            }
            
            // Approach 2: Try creating socket with specific local address
            if (!socketCreated) {
                try {
                    Log.i(TAG, "Approach 2: Trying with specific local address...")
                    
                    // Get WiFi IP address
                    val wifiManager = context.getSystemService(Context.WIFI_SERVICE) as? WifiManager
                    val wifiInfo = wifiManager?.connectionInfo
                    val ipInt = wifiInfo?.ipAddress ?: 0
                    
                    if (ipInt != 0) {
                        val ipAddress = String.format(
                            "%d.%d.%d.%d",
                            ipInt and 0xff,
                            (ipInt shr 8) and 0xff,
                            (ipInt shr 16) and 0xff,
                            (ipInt shr 24) and 0xff
                        )
                        Log.i(TAG, "WiFi IP address: $ipAddress")
                        
                        val localAddr = InetSocketAddress(InetAddress.getByName(ipAddress), 0)
                        socket = DatagramSocket(localAddr).apply {
                            soTimeout = SOCKET_TIMEOUT_MS
                            trafficClass = 0x10
                            sendBufferSize = 64 * 1024
                        }
                        socketCreated = true
                        Log.i(TAG, "SUCCESS: Socket created with local WiFi address!")
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "Local address approach failed", e)
                    lastException = e
                }
            }
            
            // Approach 3: Try unbound socket approach
            if (!socketCreated) {
                try {
                    Log.i(TAG, "Approach 3: Trying unbound socket...")
                    socket = DatagramSocket(null).apply {
                        bind(null)
                        soTimeout = SOCKET_TIMEOUT_MS
                        trafficClass = 0x10
                        sendBufferSize = 64 * 1024
                    }
                    socketCreated = true
                    Log.i(TAG, "SUCCESS: Unbound socket approach worked!")
                } catch (e: Exception) {
                    Log.w(TAG, "Unbound socket approach failed", e)
                    lastException = e
                }
            }
            
            // Approach 4: Last resort - basic socket
            if (!socketCreated) {
                try {
                    Log.i(TAG, "Approach 4: Last resort - basic socket...")
                    socket = DatagramSocket().apply {
                        soTimeout = SOCKET_TIMEOUT_MS
                        trafficClass = 0x10
                        sendBufferSize = 64 * 1024
                    }
                    socketCreated = true
                    Log.i(TAG, "SUCCESS: Basic socket approach worked!")
                } catch (e: Exception) {
                    Log.e(TAG, "All socket creation approaches failed", e)
                    throw lastException ?: e
                }
            }
            
            if (socketCreated && socket != null) {
                Log.i(TAG, "Socket created successfully!")
                Log.i(TAG, "Socket local address: ${socket!!.localAddress?.hostAddress}:${socket!!.localPort}")
                Log.i(TAG, "Socket is bound: ${socket!!.isBound}, closed: ${socket!!.isClosed}")
            }
            
            isRunning.set(true)
            Log.i(TAG, "UDP sender started successfully - target: $targetHost:$targetPort")
            Log.i(TAG, "MulticastLock status: ${multicastLock?.isHeld ?: "null"}")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start UDP sender", e)
            cleanup()
            false
        }
    }
    
    /**
     * Stop the UDP sender
     */
    fun stop() {
        if (!isRunning.getAndSet(false)) {
            return
        }
        
        Log.i(TAG, "Stopping UDP sender...")
        
        // Cancel all coroutines
        senderScope.cancel()
        
        cleanup()
        
        Log.i(TAG, "UDP sender stopped")
    }
    
    /**
     * Send audio packet asynchronously
     */
    fun sendPacketAsync(packet: AudioPacket): Job? {
        if (!isRunning.get()) {
            Log.w(TAG, "Cannot send packet - sender not running")
            return null
        }
        
        return senderScope.launch {
            sendPacketInternal(packet)
        }
    }
    
    /**
     * Send audio packet synchronously
     */
    fun sendPacket(packet: AudioPacket): Boolean {
        if (!isRunning.get()) {
            Log.w(TAG, "Cannot send packet - sender not running")
            return false
        }
        
        return sendPacketInternal(packet)
    }
    
    /**
     * Internal packet sending implementation
     */
    private fun sendPacketInternal(packet: AudioPacket): Boolean {
        return try {
            Log.v(TAG, "Starting packet send - Seq: ${packet.sequenceId}")
            
            // Check if we're still running
            if (!isRunning.get()) {
                Log.w(TAG, "Cannot send packet - sender not running")
                sendErrors.incrementAndGet()
                return false
            }
            
            // Check MulticastLock status
            val lockStatus = multicastLock?.isHeld ?: false
            Log.v(TAG, "MulticastLock status before send: $lockStatus")
            
            val data = packet.serialize()
            Log.v(TAG, "Packet serialized - Size: ${data.size} bytes")
            
            if (data.size > MAX_PACKET_SIZE) {
                Log.w(TAG, "Packet too large: ${data.size} bytes (max: $MAX_PACKET_SIZE)")
                sendErrors.incrementAndGet()
                return false
            }
            
            // Check socket status
            val currentSocket = socket
            if (currentSocket == null) {
                Log.e(TAG, "Socket is null - cannot send packet")
                sendErrors.incrementAndGet()
                return false
            }
            
            if (currentSocket.isClosed) {
                Log.e(TAG, "Socket is closed - cannot send packet")
                sendErrors.incrementAndGet()
                return false
            }
            
            Log.v(TAG, "Creating DatagramPacket for target: ${targetAddress?.address?.hostAddress}:${targetAddress?.port}")
            val datagramPacket = DatagramPacket(
                data, 
                data.size, 
                targetAddress
            )
            
            Log.v(TAG, "Attempting to send UDP packet...")
            currentSocket.send(datagramPacket)
            Log.v(TAG, "UDP packet sent successfully!")
            
            // Update statistics
            packetsSent.incrementAndGet()
            bytesSent.addAndGet(data.size.toLong())
            
            Log.v(TAG, "Packet sent successfully - Seq: ${packet.sequenceId}, Size: ${data.size} bytes")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send packet - Seq: ${packet.sequenceId}", e)
            Log.e(TAG, "Exception type: ${e.javaClass.simpleName}")
            Log.e(TAG, "Exception message: ${e.message}")
            Log.e(TAG, "Socket status - closed: ${socket?.isClosed}, bound: ${socket?.isBound}")
            Log.e(TAG, "MulticastLock status: ${multicastLock?.isHeld ?: "null"}")
            sendErrors.incrementAndGet()
            false
        }
    }
    
    /**
     * Get statistics
     */
    fun getPacketsSent(): Long = packetsSent.get()
    fun getBytesSent(): Long = bytesSent.get()
    fun getSendErrors(): Long = sendErrors.get()
    fun isRunning(): Boolean = isRunning.get()
    
    /**
     * Reset statistics
     */
    fun resetStats() {
        packetsSent.set(0)
        bytesSent.set(0)
        sendErrors.set(0)
    }
    
    /**
     * Cleanup resources
     */
    private fun cleanup() {
        try {
            socket?.close()
        } catch (e: Exception) {
            Log.w(TAG, "Error closing socket", e)
        }
        socket = null
        targetAddress = null
        
        // Release WiFi MulticastLock
        try {
            multicastLock?.let { lock ->
                Log.i(TAG, "Checking MulticastLock for release - isHeld: ${lock.isHeld}")
                if (lock.isHeld) {
                    lock.release()
                    Log.i(TAG, "WiFi MulticastLock released successfully")
                } else {
                    Log.w(TAG, "MulticastLock was not held during cleanup")
                }
            } ?: Log.w(TAG, "MulticastLock was null during cleanup")
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing WiFi MulticastLock", e)
        }
        multicastLock = null
        Log.i(TAG, "MulticastLock set to null")
    }
}