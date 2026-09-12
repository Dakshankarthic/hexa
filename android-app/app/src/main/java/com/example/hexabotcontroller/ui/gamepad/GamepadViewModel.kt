package com.example.hexabotcontroller.ui.gamepad

import android.app.Application
import android.bluetooth.BluetoothDevice
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.example.hexabotcontroller.bluetooth.BluetoothService
import com.example.hexabotcontroller.bluetooth.ConnectionState
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

class GamepadViewModel(application: Application) : AndroidViewModel(application) {

    private val btService = BluetoothService(application)

    val connectionState: StateFlow<ConnectionState> = btService.connectionState
    val connectedDeviceName: StateFlow<String?> = btService.connectedDeviceName
    val lastResponse: StateFlow<String> = btService.lastResponse

    val isBluetoothAvailable: Boolean get() = btService.isBluetoothAvailable
    val isBluetoothEnabled: Boolean get() = btService.isBluetoothEnabled

    private val _pairedDevices = MutableStateFlow<List<BluetoothDevice>>(emptyList())
    val pairedDevices: StateFlow<List<BluetoothDevice>> = _pairedDevices.asStateFlow()

    private val _lastCommand = MutableStateFlow("")
    val lastCommand: StateFlow<String> = _lastCommand.asStateFlow()

    fun loadPairedDevices() {
        _pairedDevices.value = btService.getPairedDevices()
    }

    fun connectToDevice(device: BluetoothDevice) {
        viewModelScope.launch {
            btService.connect(device)
        }
    }

    fun disconnect() {
        btService.disconnect()
    }

    fun sendCommand(command: Char) {
        _lastCommand.value = when (command) {
            'F' -> "FORWARD"
            'B' -> "BACKWARD"
            'L' -> "LEFT"
            'R' -> "RIGHT"
            'X' -> "STOP"
            'U' -> "STAND UP"
            'D' -> "SIT DOWN"
            else -> command.toString()
        }
        viewModelScope.launch {
            btService.sendCommand(command)
        }
    }

    override fun onCleared() {
        super.onCleared()
        btService.disconnect()
    }
}
