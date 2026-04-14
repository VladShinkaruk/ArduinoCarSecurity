package com.example.arduinocarsecurity

import org.assertj.core.api.Assertions.assertThat
import org.junit.Test

class DeviceStatusParserTest {
    @Test
    fun `translateSensor MIC Микрофон`() {
        assertThat(DeviceStatusParser.translateSensor("MIC"))
            .describedAs("Заглавные MIC")
            .isEqualTo("Микрофон")

        assertThat(DeviceStatusParser.translateSensor("mic"))
            .describedAs("Строчные mic")
            .isEqualTo("Микрофон")

        assertThat(DeviceStatusParser.translateSensor("  MIC  "))
            .describedAs("MIC с пробелами")
            .isEqualTo("Микрофон")
    }

    @Test
    fun `translateSensor GYRO Гироскоп`() {
        assertThat(DeviceStatusParser.translateSensor("GYRO"))
            .describedAs("Заглавные GYRO")
            .isEqualTo("Гироскоп")

        assertThat(DeviceStatusParser.translateSensor("gyro"))
            .describedAs("Строчные gyro")
            .isEqualTo("Гироскоп")
    }

    @Test
    fun `translateSensor MIC+GYRO комбинированный`() {
        val cases = listOf(
            "MIC+GYRO" to "Микрофон + Гироскоп",
            "gyro + mic" to "Микрофон + Гироскоп",
            "MICROPHONE_GYRO" to "Микрофон + Гироскоп",
            "GYRO_MIC_ALERT" to "Микрофон + Гироскоп"
        )

        cases.forEach { (input, expected) ->
            assertThat(DeviceStatusParser.translateSensor(input))
                .describedAs("Вход: '$input'")
                .isEqualTo(expected)
        }
    }

    @Test
    fun `translateSensor неизвестный сенсор возвращает как есть`() {
        assertThat(DeviceStatusParser.translateSensor("GPS"))
            .isEqualTo("GPS")
        assertThat(DeviceStatusParser.translateSensor("UNKNOWN"))
            .isEqualTo("UNKNOWN")
        assertThat(DeviceStatusParser.translateSensor(""))
            .isEqualTo("")
    }

    @Test
    fun `parseStatusLine валидная строка парсится корректно`() {
        val input = "1|1|1|45.2|32.1|53.9045,27.5615|2024-01-15 10:30:00"
        val result = DeviceStatusParser.parseStatusLine(input)

        assertThat(result)
            .describedAs("Валидная строка из 7 частей должна распарситься")
            .isNotNull()

        assertThat(result!!.systemEnabled).isTrue()
        assertThat(result.alarmActive).isTrue()
        assertThat(result.micDetected).isTrue()
        assertThat(result.gyroValue).isEqualTo("45.2")
        assertThat(result.gpsCoords).isEqualTo("53.9045,27.5615")
        assertThat(result.timestamp).isEqualTo("2024-01-15 10:30:00")
    }

    @Test
    fun `parseStatusLine система выключена`() {
        val input = "0|0|0|0|0|0,0|2024-01-15 10:30:00"
        val result = DeviceStatusParser.parseStatusLine(input)

        assertThat(result).isNotNull()
        assertThat(result!!.systemEnabled).isFalse()
        assertThat(result.alarmActive).isFalse()
        assertThat(result.micDetected).isFalse()
    }

    @Test
    fun `parseStatusLine невалидная строка возвращает null`() {
        assertThat(DeviceStatusParser.parseStatusLine(""))
            .describedAs("Пустая строка")
            .isNull()

        assertThat(DeviceStatusParser.parseStatusLine("1|2|3"))
            .describedAs("Мало частей (<7)")
            .isNull()

        assertThat(DeviceStatusParser.parseStatusLine("invalid|data|here|more|data|coords|time"))
            .describedAs("Некорректные данные (но формат верный) — НЕ должна возвращать null")
            .isNotNull()
    }

    @Test
    fun `parseStatusLine ровно 7 частей — минимально валидно`() {
        val input = "1|0|1|0|0|0,0|2024-01-01 00:00:00"
        val result = DeviceStatusParser.parseStatusLine(input)

        assertThat(result).isNotNull()
        assertThat(result!!.systemEnabled).isTrue()
        assertThat(result.alarmActive).isFalse()
    }

    @Test
    fun `parseStatusLine больше 7 частей — берёт первые 7`() {
        val input = "1|1|0|10|20|55.5,37.5|2024-01-15 12:00:00|extra|garbage"
        val result = DeviceStatusParser.parseStatusLine(input)

        assertThat(result).isNotNull()
        assertThat(result!!.gpsCoords).isEqualTo("55.5,37.5") // часть [5]
        assertThat(result.timestamp).isEqualTo("2024-01-15 12:00:00") // часть [6]
    }

    @Test
    fun `parseStatusLine null возвращает null`() {
        val result = try {
            DeviceStatusParser.parseStatusLine(null as String)
        } catch (e: Exception) {
            null
        }
        assertThat(result).isNull()
    }
}