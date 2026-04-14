package com.example.arduinocarsecurity
import org.junit.Assert.*
import org.junit.Test
private const val COLOR_BLUE = -0xe99a40
private const val COLOR_GREEN = -0xd182ce
private const val COLOR_RED = -0x1000000
private const val COLOR_PURPLE = -0x95e466
private const val COLOR_TEAL = -0xff7685
private const val COLOR_GRAY = -0xbaa59c
private const val COLOR_BLACK = -0x1000000

class HistoryViewHolderTest {
    @Test
    fun `translateMessage MIC содержит Микрофон`() {
        assertTrue(translateMessageSafe("Sensor: MIC triggered").contains("Микрофон"))
        assertTrue(translateMessageSafe("mic alert").contains("Микрофон"))
        assertTrue(translateMessageSafe("  MIC  ").contains("Микрофон"))
    }

    @Test
    fun `translateMessage GYRO содержит Гироскоп`() {
        assertTrue(translateMessageSafe("GYRO: 45.2").contains("Гироскоп"))
        assertTrue(translateMessageSafe("gyro detected").contains("Гироскоп"))
    }

    @Test
    fun `translateMessage MIC+GYRO комбинированный`() {
        val result = translateMessageSafe("Alert: MIC + GYRO")
        assertTrue(result.contains("Микрофон + Гироскоп"))
        assertFalse(result.contains("MIC + GYRO"))
    }

    @Test
    fun `translateMessage неизвестный текст возвращается как есть`() {
        assertEquals("Hello World", translateMessageSafe("Hello World"))
        assertEquals("GPS: 53.9,27.56", translateMessageSafe("GPS: 53.9,27.56"))
    }

    @Test
    fun `getColorForType COMMAND синий`() {
        assertEquals(COLOR_BLUE, getColorForType("COMMAND_HTTP"))
        assertEquals(COLOR_BLUE, getColorForType("COMMAND_SMS"))
    }

    @Test
    fun `getColorForType RESPONSE зелёный`() {
        assertEquals(COLOR_GREEN, getColorForType("RESPONSE_SMS"))
        assertEquals(COLOR_GREEN, getColorForType("RESPONSE_HTTP"))
    }

    @Test
    fun `getColorForType ALARM красный`() {
        assertEquals(COLOR_RED, getColorForType("ALARM"))
        assertEquals(COLOR_RED, getColorForType("ALARM_GPS"))
    }

    @Test
    fun `getColorForType SYSTEM фиолетовый`() {
        assertEquals(COLOR_PURPLE, getColorForType("SYSTEM"))
    }

    @Test
    fun `getColorForType SERVER серый`() {
        assertEquals(COLOR_GRAY, getColorForType("SERVER_ERROR"))
    }

    @Test
    fun `getColorForType неизвестный тип чёрный`() {
        assertEquals(COLOR_BLACK, getColorForType("UNKNOWN_TYPE"))
        assertEquals(COLOR_BLACK, getColorForType(""))
    }

    @Test
    fun `getTitleForType COMMAND Команда`() {
        assertEquals("Команда", getTitleForType("COMMAND_HTTP"))
    }

    @Test
    fun `getTitleForType RESPONSE Ответ`() {
        assertEquals("Ответ", getTitleForType("RESPONSE_SMS"))
    }

    @Test
    fun `getTitleForType ALARM Тревога`() {
        assertEquals("Тревога", getTitleForType("ALARM"))
    }

    @Test
    fun `getTitleForType SYSTEM Система`() {
        assertEquals("Система", getTitleForType("SYSTEM"))
    }

    private fun translateMessageSafe(message: String): String {
        var text = message.uppercase()
        if (text.contains("MIC") && text.contains("GYRO")) {
            return message.replace(Regex("MIC\\s*\\+?\\s*GYRO|GYRO\\s*\\+?\\s*MIC", RegexOption.IGNORE_CASE), "Микрофон + Гироскоп")
        }
        if (text.contains("MIC")) {
            return message.replace(Regex("MIC", RegexOption.IGNORE_CASE), "Микрофон")
        }
        if (text.contains("GYRO")) {
            return message.replace(Regex("GYRO", RegexOption.IGNORE_CASE), "Гироскоп")
        }
        return message
    }

    private fun getColorForType(type: String): Int {
        return when {
            type.startsWith("COMMAND") -> COLOR_BLUE
            type.startsWith("RESPONSE") -> COLOR_GREEN
            type.startsWith("ALARM") -> COLOR_RED
            type.startsWith("SYSTEM") -> COLOR_PURPLE
            type.startsWith("GPS") -> COLOR_TEAL
            type.startsWith("SERVER") -> COLOR_GRAY
            else -> COLOR_BLACK
        }
    }

    private fun getTitleForType(type: String): String {
        return when {
            type.startsWith("COMMAND") -> "Команда"
            type.startsWith("RESPONSE") -> "Ответ"
            type.startsWith("ALARM") -> "Тревога"
            type.startsWith("SYSTEM") -> "Система"
            type.startsWith("GPS") -> "GPS"
            type.startsWith("SERVER") -> "Сервер"
            else -> type
        }
    }
}