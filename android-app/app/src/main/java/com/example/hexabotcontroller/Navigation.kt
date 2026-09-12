package com.example.hexabotcontroller

import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.hexabotcontroller.ui.gamepad.GamepadScreen
import com.example.hexabotcontroller.ui.gamepad.GamepadViewModel

@Composable
fun MainNavigation() {
    val viewModel: GamepadViewModel = viewModel()

    val connectionState by viewModel.connectionState.collectAsState()
    val connectedDeviceName by viewModel.connectedDeviceName.collectAsState()
    val lastCommand by viewModel.lastCommand.collectAsState()
    val lastResponse by viewModel.lastResponse.collectAsState()
    val pairedDevices by viewModel.pairedDevices.collectAsState()

    GamepadScreen(
        connectionState = connectionState,
        connectedDeviceName = connectedDeviceName,
        lastCommand = lastCommand,
        lastResponse = lastResponse,
        pairedDevices = pairedDevices,
        onLoadDevices = { viewModel.loadPairedDevices() },
        onConnect = { device -> viewModel.connectToDevice(device) },
        onDisconnect = { viewModel.disconnect() },
        onCommand = { cmd -> viewModel.sendCommand(cmd) },
        modifier = Modifier
    )
}
