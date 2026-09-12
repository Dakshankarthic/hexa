package com.example.hexabotcontroller.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable

private val HexaDarkColorScheme = darkColorScheme(
    primary = HexaCyan,
    onPrimary = DarkBg,
    primaryContainer = HexaCyanDim,
    onPrimaryContainer = HexaCyan,
    secondary = NeonBlue,
    onSecondary = DarkBg,
    tertiary = NeonOrange,
    onTertiary = DarkBg,
    background = DarkBg,
    onBackground = TextWhite,
    surface = DarkSurface,
    onSurface = TextWhite,
    surfaceVariant = DarkCard,
    onSurfaceVariant = TextGray,
    error = NeonRed,
    onError = DarkBg,
)

@Composable
fun HexaBotControllerTheme(
    content: @Composable () -> Unit,
) {
    MaterialTheme(
        colorScheme = HexaDarkColorScheme,
        typography = Typography,
        content = content
    )
}
