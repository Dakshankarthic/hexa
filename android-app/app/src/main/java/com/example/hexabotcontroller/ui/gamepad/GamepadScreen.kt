package com.example.hexabotcontroller.ui.gamepad

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.hexabotcontroller.bluetooth.ConnectionState
import com.example.hexabotcontroller.theme.*

// =============================================================================
// GAMEPAD SCREEN - Main controller UI
// =============================================================================
@Composable
fun GamepadScreen(
    connectionState: ConnectionState,
    connectedDeviceName: String?,
    lastCommand: String,
    lastResponse: String,
    pairedDevices: List<BluetoothDevice>,
    onLoadDevices: () -> Unit,
    onConnect: (BluetoothDevice) -> Unit,
    onDisconnect: () -> Unit,
    onCommand: (Char) -> Unit,
    modifier: Modifier = Modifier
) {
    val isConnected = connectionState == ConnectionState.CONNECTED

    Box(
        modifier = modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(DarkBg, DarkSurface, DarkBg)
                )
            )
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 24.dp)
                .safeDrawingPadding(),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Spacer(modifier = Modifier.height(16.dp))

            // Header
            HeaderSection(
                connectionState = connectionState,
                deviceName = connectedDeviceName
            )

            Spacer(modifier = Modifier.height(8.dp))

            if (isConnected) {
                // Status bar
                StatusBar(lastCommand = lastCommand, lastResponse = lastResponse)

                Spacer(modifier = Modifier.weight(0.3f))

                // D-Pad
                DPadSection(onCommand = onCommand)

                Spacer(modifier = Modifier.weight(0.2f))

                // Action buttons
                ActionButtonsSection(onCommand = onCommand)

                Spacer(modifier = Modifier.weight(0.2f))

                // Disconnect button
                TextButton(
                    onClick = onDisconnect,
                    modifier = Modifier.padding(bottom = 8.dp)
                ) {
                    Text(
                        "Disconnect",
                        color = NeonRed,
                        fontSize = 14.sp
                    )
                }
            } else {
                // Connection screen
                Spacer(modifier = Modifier.height(32.dp))
                ConnectionScreen(
                    connectionState = connectionState,
                    pairedDevices = pairedDevices,
                    onLoadDevices = onLoadDevices,
                    onConnect = onConnect
                )
            }
        }
    }
}

// =============================================================================
// HEADER with connection indicator
// =============================================================================
@Composable
private fun HeaderSection(
    connectionState: ConnectionState,
    deviceName: String?
) {
    val dotColor by animateColorAsState(
        targetValue = when (connectionState) {
            ConnectionState.CONNECTED -> NeonGreen
            ConnectionState.CONNECTING -> NeonOrange
            ConnectionState.ERROR -> NeonRed
            ConnectionState.DISCONNECTED -> TextDim
        },
        animationSpec = tween(500),
        label = "dotColor"
    )

    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            text = "🕷️ HEXA-BOT",
            fontSize = 28.sp,
            fontWeight = FontWeight.Bold,
            color = HexaCyan,
            letterSpacing = 2.sp
        )

        Spacer(modifier = Modifier.height(8.dp))

        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center
        ) {
            Box(
                modifier = Modifier
                    .size(10.dp)
                    .clip(CircleShape)
                    .background(dotColor)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = when (connectionState) {
                    ConnectionState.CONNECTED -> "Connected to $deviceName"
                    ConnectionState.CONNECTING -> "Connecting..."
                    ConnectionState.ERROR -> "Connection failed"
                    ConnectionState.DISCONNECTED -> "Disconnected"
                },
                color = TextGray,
                fontSize = 13.sp
            )
        }
    }
}

// =============================================================================
// STATUS BAR - shows last command and response
// =============================================================================
@Composable
private fun StatusBar(lastCommand: String, lastResponse: String) {
    if (lastCommand.isEmpty()) return

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(DarkCard.copy(alpha = 0.7f))
            .border(1.dp, HexaCyanDim.copy(alpha = 0.3f), RoundedCornerShape(12.dp))
            .padding(horizontal = 16.dp, vertical = 10.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "⚡ $lastCommand",
                color = HexaCyan,
                fontSize = 13.sp,
                fontWeight = FontWeight.Medium
            )
            if (lastResponse.isNotEmpty()) {
                Text(
                    text = lastResponse,
                    color = NeonGreen,
                    fontSize = 12.sp
                )
            }
        }
    }
}

// =============================================================================
// D-PAD SECTION
// =============================================================================
@Composable
private fun DPadSection(onCommand: (Char) -> Unit) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Forward button
        DPadButton(
            label = "▲",
            pressCommand = 'F',
            releaseCommand = 'X',
            onCommand = onCommand
        )

        Spacer(modifier = Modifier.height(12.dp))

        // Left - Stop - Right
        Row(
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            DPadButton(
                label = "◄",
                pressCommand = 'L',
                releaseCommand = 'X',
                onCommand = onCommand
            )

            Spacer(modifier = Modifier.width(12.dp))

            // STOP button (center)
            StopButton(onCommand = onCommand)

            Spacer(modifier = Modifier.width(12.dp))

            DPadButton(
                label = "►",
                pressCommand = 'R',
                releaseCommand = 'X',
                onCommand = onCommand
            )
        }

        Spacer(modifier = Modifier.height(12.dp))

        // Backward button
        DPadButton(
            label = "▼",
            pressCommand = 'B',
            releaseCommand = 'X',
            onCommand = onCommand
        )
    }
}

// =============================================================================
// D-PAD BUTTON - sends command on press, stop on release
// =============================================================================
@Composable
private fun DPadButton(
    label: String,
    pressCommand: Char,
    releaseCommand: Char,
    onCommand: (Char) -> Unit
) {
    var isPressed by remember { mutableStateOf(false) }

    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.9f else 1.0f,
        animationSpec = tween(100),
        label = "scale"
    )

    val bgColor by animateColorAsState(
        targetValue = if (isPressed) HexaCyan else DarkCard,
        animationSpec = tween(100),
        label = "bgColor"
    )

    val textColor by animateColorAsState(
        targetValue = if (isPressed) DarkBg else TextWhite,
        animationSpec = tween(100),
        label = "textColor"
    )

    Box(
        modifier = Modifier
            .size(85.dp)
            .scale(scale)
            .shadow(if (isPressed) 0.dp else 8.dp, RoundedCornerShape(16.dp))
            .clip(RoundedCornerShape(16.dp))
            .background(bgColor)
            .border(
                width = 1.5f.dp,
                color = if (isPressed) HexaCyan else HexaCyanDim.copy(alpha = 0.4f),
                shape = RoundedCornerShape(16.dp)
            )
            .pointerInput(Unit) {
                detectTapGestures(
                    onPress = {
                        isPressed = true
                        onCommand(pressCommand)
                        tryAwaitRelease()
                        isPressed = false
                        onCommand(releaseCommand)
                    }
                )
            },
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = label,
            fontSize = 32.sp,
            fontWeight = FontWeight.Bold,
            color = textColor
        )
    }
}

// =============================================================================
// STOP BUTTON (center of D-pad)
// =============================================================================
@Composable
private fun StopButton(onCommand: (Char) -> Unit) {
    var isPressed by remember { mutableStateOf(false) }

    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.9f else 1.0f,
        animationSpec = tween(100),
        label = "stopScale"
    )

    Box(
        modifier = Modifier
            .size(85.dp)
            .scale(scale)
            .shadow(if (isPressed) 0.dp else 8.dp, CircleShape)
            .clip(CircleShape)
            .background(
                if (isPressed) NeonRedDark else NeonRed
            )
            .pointerInput(Unit) {
                detectTapGestures(
                    onPress = {
                        isPressed = true
                        onCommand('X')
                        tryAwaitRelease()
                        isPressed = false
                    }
                )
            },
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = "STOP",
            fontSize = 16.sp,
            fontWeight = FontWeight.ExtraBold,
            color = Color.White,
            letterSpacing = 1.sp
        )
    }
}

// =============================================================================
// ACTION BUTTONS (Stand Up / Sit Down)
// =============================================================================
@Composable
private fun ActionButtonsSection(onCommand: (Char) -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(16.dp, Alignment.CenterHorizontally)
    ) {
        ActionButton(
            label = "🦿 Stand Up",
            command = 'U',
            color = NeonBlue,
            onCommand = onCommand,
            modifier = Modifier.weight(1f)
        )
        ActionButton(
            label = "🪑 Sit Down",
            command = 'D',
            color = TextDim,
            onCommand = onCommand,
            modifier = Modifier.weight(1f)
        )
    }
}

@Composable
private fun ActionButton(
    label: String,
    command: Char,
    color: Color,
    onCommand: (Char) -> Unit,
    modifier: Modifier = Modifier
) {
    var isPressed by remember { mutableStateOf(false) }

    val scale by animateFloatAsState(
        targetValue = if (isPressed) 0.95f else 1.0f,
        animationSpec = tween(100),
        label = "actionScale"
    )

    Box(
        modifier = modifier
            .height(56.dp)
            .scale(scale)
            .clip(RoundedCornerShape(14.dp))
            .background(color.copy(alpha = if (isPressed) 1f else 0.8f))
            .pointerInput(Unit) {
                detectTapGestures(
                    onPress = {
                        isPressed = true
                        onCommand(command)
                        tryAwaitRelease()
                        isPressed = false
                    }
                )
            },
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = label,
            fontSize = 16.sp,
            fontWeight = FontWeight.Bold,
            color = Color.White
        )
    }
}

// =============================================================================
// CONNECTION SCREEN - shown when not connected
// =============================================================================
@SuppressLint("MissingPermission")
@Composable
private fun ConnectionScreen(
    connectionState: ConnectionState,
    pairedDevices: List<BluetoothDevice>,
    onLoadDevices: () -> Unit,
    onConnect: (BluetoothDevice) -> Unit
) {
    LaunchedEffect(Unit) {
        onLoadDevices()
    }

    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Spider icon
        Text(
            text = "🕷️",
            fontSize = 80.sp,
            modifier = Modifier.padding(bottom = 16.dp)
        )

        Text(
            text = "Connect to your Hexapod",
            fontSize = 20.sp,
            fontWeight = FontWeight.SemiBold,
            color = TextWhite
        )

        Text(
            text = "Select a paired Bluetooth device",
            fontSize = 14.sp,
            color = TextGray,
            modifier = Modifier.padding(top = 4.dp, bottom = 24.dp)
        )

        if (connectionState == ConnectionState.CONNECTING) {
            CircularProgressIndicator(
                color = HexaCyan,
                modifier = Modifier.padding(24.dp)
            )
            Text(
                text = "Connecting...",
                color = HexaCyan,
                fontSize = 14.sp
            )
        } else if (connectionState == ConnectionState.ERROR) {
            Text(
                text = "⚠️ Connection failed. Make sure HEXA-BOT is powered on and paired.",
                color = NeonRed,
                fontSize = 14.sp,
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(bottom = 16.dp)
            )
        }

        // Device list
        if (pairedDevices.isEmpty()) {
            Text(
                text = "No paired devices found.\nGo to Settings → Bluetooth and pair with HEXA-BOT first.",
                color = TextGray,
                fontSize = 13.sp,
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(24.dp)
            )
        } else {
            LazyColumn(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f, fill = false),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(pairedDevices) { device ->
                    DeviceItem(
                        device = device,
                        isConnecting = connectionState == ConnectionState.CONNECTING,
                        onClick = { onConnect(device) }
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        // Refresh button
        OutlinedButton(
            onClick = onLoadDevices,
            colors = ButtonDefaults.outlinedButtonColors(contentColor = HexaCyan),
            modifier = Modifier.padding(bottom = 16.dp)
        ) {
            Text("🔄  Refresh Devices")
        }
    }
}

@SuppressLint("MissingPermission")
@Composable
private fun DeviceItem(
    device: BluetoothDevice,
    isConnecting: Boolean,
    onClick: () -> Unit
) {
    val isHexaBot = device.name?.contains("HEXA", ignoreCase = true) == true

    Button(
        onClick = onClick,
        enabled = !isConnecting,
        shape = RoundedCornerShape(14.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = if (isHexaBot) HexaCyanDim.copy(alpha = 0.3f) else DarkCard,
            contentColor = TextWhite
        ),
        modifier = Modifier
            .fillMaxWidth()
            .height(64.dp)
            .then(
                if (isHexaBot) Modifier.border(
                    1.dp,
                    HexaCyan.copy(alpha = 0.5f),
                    RoundedCornerShape(14.dp)
                )
                else Modifier
            )
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(
                    text = device.name ?: "Unknown Device",
                    fontSize = 16.sp,
                    fontWeight = if (isHexaBot) FontWeight.Bold else FontWeight.Normal,
                    color = if (isHexaBot) HexaCyan else TextWhite
                )
                Text(
                    text = device.address,
                    fontSize = 11.sp,
                    color = TextDim
                )
            }
            if (isHexaBot) {
                Text(
                    text = "🕷️",
                    fontSize = 24.sp
                )
            }
        }
    }
}
