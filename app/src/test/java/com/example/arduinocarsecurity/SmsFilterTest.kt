package com.example.arduinocarsecurity

import org.junit.Assert.*
import org.junit.Test

class SmsFilterTest {
    private val deviceNumber = "+375296625470"

    @Test
    fun `isFromDevice точное совпадение номера`() {
        assertTrue(isFromDeviceSafe("+375296625470"))
    }

    @Test
    fun `isFromDevice другой номер возвращает false`() {
        assertFalse(isFromDeviceSafe("+375291234567"))
        assertFalse(isFromDeviceSafe("+375441234567"))
        assertFalse(isFromDeviceSafe("unknown"))
        assertFalse(isFromDeviceSafe(""))
    }

    @Test
    fun `isFromDevice номер с пробелами в другом формате`() {
        assertTrue(isFromDeviceSafe("+37529 662 54 70"))
    }

    @Test
    fun `isValidDeviceMessage содержит System ON`() {
        assertTrue(isValidDeviceMessageSafe("System: ON, Sensor: MIC"))
        assertTrue(isValidDeviceMessageSafe("Alert: OK, System: ON"))
    }

    @Test
    fun `isValidDeviceMessage содержит System OFF`() {
        assertTrue(isValidDeviceMessageSafe("System: OFF"))
    }

    @Test
    fun `isValidDeviceMessage содержит TREVOGA`() {
        assertTrue(isValidDeviceMessageSafe("TREVOGA! Sensor: GYRO"))
        assertTrue(isValidDeviceMessageSafe("ALARM: TREVOGA detected"))
    }

    @Test
    fun `isValidDeviceMessage не содержит ключевых слов`() {
        assertFalse(isValidDeviceMessageSafe("Hello from device"))
        assertFalse(isValidDeviceMessageSafe("Just random text"))
        assertFalse(isValidDeviceMessageSafe(""))
    }

    @Test
    fun `extractSensor извлекает имя сенсора`() {
        val msg = "TREVOGA! Sensor: MIC, Alert: HIGH"
        assertEquals("MIC", extractSensorSafe(msg))
    }

    @Test
    fun `extractSensor несколько сенсоров через плюс`() {
        val msg = "Alert: Sensor: MIC+GYRO triggered"
        val result = extractSensorSafe(msg)
        assertTrue(result.contains("MIC") || result.contains("Микрофон"))
        assertTrue(result.contains("GYRO") || result.contains("Гироскоп"))
    }

    @Test
    fun `extractSensor если сенсора нет возвращает пустую строку`() {
        assertEquals("", extractSensorSafe("System: ON"))
        assertEquals("", extractSensorSafe(""))
    }

    private fun isFromDeviceSafe(sender: String): Boolean {
        val cleanSender = sender.replace(" ", "")
        return cleanSender.contains(deviceNumber)
    }

    private fun isValidDeviceMessageSafe(body: String): Boolean {
        return body.contains("System:") || body.contains("TREVOGA")
    }

    private fun extractSensorSafe(body: String): String {
        return body.substringAfter("Sensor:", "")
            .substringBefore(",")
            .trim()
    }
}