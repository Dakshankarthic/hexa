package com.example.hexabotcontroller.bluetooth

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID

/**
 * Connection state for the Bluetooth link to HEXA-BOT.
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
}

/**
 * Manages Bluetooth Classic SPP connection to the ESP32 hexapod.
 */
class BluetoothService(context: Context) {

    companion object {
        // Standard SPP UUID for Bluetooth Serial
        val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    }

    private val bluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter

    private var socket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null
    private var inputStream: InputStream? = null

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _connectedDeviceName = MutableStateFlow<String?>(null)
    val connectedDeviceName: StateFlow<String?> = _connectedDeviceName.asStateFlow()

    private val _lastResponse = MutableStateFlow("")
    val lastResponse: StateFlow<String> = _lastResponse.asStateFlow()

    val isBluetoothAvailable: Boolean
        get() = bluetoothAdapter != null

    val isBluetoothEnabled: Boolean
        get() = bluetoothAdapter?.isEnabled == true

    /**
     * Returns a list of paired Bluetooth devices.
     */
    @SuppressLint("MissingPermission")
    fun getPairedDevices(): List<BluetoothDevice> {
        return bluetoothAdapter?.bondedDevices?.toList() ?: emptyList()
    }

    /**
     * Connect to a paired Bluetooth device (runs on IO dispatcher).
     */
    @SuppressLint("MissingPermission")
    suspend fun connect(device: BluetoothDevice) {
        if (_connectionState.value == ConnectionState.CONNECTING) return

        _connectionState.value = ConnectionState.CONNECTING

        withContext(Dispatchers.IO) {
            try {
                // Cancel discovery to speed up connection
                bluetoothAdapter?.cancelDiscovery()

                // Create SPP socket and connect
                socket = device.createRfcommSocketToServiceRecord(SPP_UUID)
                socket?.connect()

                outputStream = socket?.outputStream
                inputStream = socket?.inputStream

                _connectedDeviceName.value = device.name ?: "Unknown"
                _connectionState.value = ConnectionState.CONNECTED

                // Start reading responses in background
                readResponses()
            } catch (e: IOException) {
                _connectionState.value = ConnectionState.ERROR
                disconnect()
            }
        }
    }

    /**
     * Send a single-character command to the ESP32.
     */
    @SuppressLint("MissingPermission")
    suspend fun sendCommand(command: Char) {
        if (_connectionState.value != ConnectionState.CONNECTED) return

        withContext(Dispatchers.IO) {
            try {
                outputStream?.write(command.code)
                outputStream?.flush()
            } catch (e: IOException) {
                _connectionState.value = ConnectionState.ERROR
                disconnect()
            }
        }
    }

    /**
     * Read responses from the ESP32 (blocking, runs on IO thread).
     */
    private fun readResponses() {
        try {
            val buffer = ByteArray(256)
            while (_connectionState.value == ConnectionState.CONNECTED) {
                val bytes = inputStream?.read(buffer) ?: -1
                if (bytes > 0) {
                    val response = String(buffer, 0, bytes).trim()
                    if (response.isNotEmpty()) {
                        _lastResponse.value = response
                    }
                } else if (bytes == -1) {
                    break
                }
            }
        } catch (_: IOException) {
            // Connection lost
        }
        if (_connectionState.value == ConnectionState.CONNECTED) {
            _connectionState.value = ConnectionState.DISCONNECTED
        }
    }

    /**
     * Disconnect and clean up resources.
     */
    fun disconnect() {
        try {
            outputStream?.close()
            inputStream?.close()
            socket?.close()
        } catch (_: IOException) { }

        outputStream = null
        inputStream = null
        socket = null
        _connectedDeviceName.value = null

        if (_connectionState.value != ConnectionState.ERROR) {
            _connectionState.value = ConnectionState.DISCONNECTED
        }
    }
}
