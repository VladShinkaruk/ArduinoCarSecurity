package com.example.arduinocarsecurity

object DeviceStatusParser {
    fun translateSensor(sensor: String): String {
        val s = sensor.trim().uppercase()
        return when {
            s.contains("MIC") && s.contains("GYRO") -> "Микрофон + Гироскоп"
            s.contains("MIC") -> "Микрофон"
            s.contains("GYRO") -> "Гироскоп"
            else -> sensor
        }
    }

    fun parseStatusLine(raw: String): ParsedStatus? {
        try {
            val parts = raw.trim().split("|")
            if (parts.size < 7) return null
            return ParsedStatus(
                systemEnabled = parts[0] == "1",
                alarmActive = parts[1] == "1",
                micDetected = parts[2] != "0",
                gyroValue = parts[3],
                gpsCoords = parts[5],
                timestamp = parts[6]
            )
        } catch (e: Exception) {
            return null
        }
    }

    data class ParsedStatus(
        val systemEnabled: Boolean,
        val alarmActive: Boolean,
        val micDetected: Boolean,
        val gyroValue: String,
        val gpsCoords: String,
        val timestamp: String
    )
}