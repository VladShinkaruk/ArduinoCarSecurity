package com.example.arduinocarsecurity

import com.yandex.mapkit.geometry.Point
import org.assertj.core.api.Assertions.assertThat
import org.junit.Test
import java.text.SimpleDateFormat
import java.util.*

class MapUtilsTest {
    @Test
    fun `parseCoords валидные координаты возвращает Point`() {
        val result = parseCoordsSafe("53.9045, 27.5615")

        assertThat(result)
            .describedAs("Координаты должны распарситься")
            .isNotNull()
        assertThat(result?.latitude).isEqualTo(53.9045)
        assertThat(result?.longitude).isEqualTo(27.5615)
    }

    @Test
    fun `parseCoords координаты без пробелов`() {
        val result = parseCoordsSafe("53.9,27.56")

        assertThat(result?.latitude).isEqualTo(53.9)
        assertThat(result?.longitude).isEqualTo(27.56)
    }

    @Test
    fun `parseCoords невалидная строка возвращает null`() {
        assertThat(parseCoordsSafe("")).isNull()
        assertThat(parseCoordsSafe("invalid")).isNull()
        assertThat(parseCoordsSafe("53.9")).isNull()
        assertThat(parseCoordsSafe("abc,def")).isNull()
        assertThat(parseCoordsSafe(null)).isNull()
    }

    @Test
    fun `parseCoords координаты с лишними символами`() {
        assertThat(parseCoordsSafe("  53.9 , 27.56  ")).isNotNull()
        assertThat(parseCoordsSafe("53.9000,27.5600")).isNotNull()
    }

    @Test
    fun `parseTime валидная дата возвращает миллисекунды`() {
        val result = parseTimeSafe("2024-01-15 10:30:00")
        assertThat(result)
            .describedAs("Время должно распарситься")
            .isGreaterThan(0)

        val expected = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
            .parse("2024-01-15 10:30:00")?.time ?: 0
        assertThat(result).isEqualTo(expected)
    }

    @Test
    fun `parseTime невалидная дата возвращает 0`() {
        assertThat(parseTimeSafe("")).isEqualTo(0)
        assertThat(parseTimeSafe("invalid")).isEqualTo(0)
        assertThat(parseTimeSafe(null)).isEqualTo(0)
    }

    @Test
    fun `formatTime форматирует миллисекунды в читаемый вид`() {
        val timestamp = 1705315800000L
        val result = formatTimeSafe(timestamp)
        assertThat(result)
            .describedAs("Формат должен быть дд.ММ чч:мм:сс")
            .matches("\\d{2}\\.\\d{2} \\d{2}:\\d{2}:\\d{2}")
    }

    @Test
    fun `formatTime для 0 возвращает текущее время`() {
        val result = formatTimeSafe(0)
        assertThat(result).isNotBlank()
        assertThat(result).matches("\\d{2}\\.\\d{2} \\d{2}:\\d{2}:\\d{2}")
    }

    private fun parseCoordsSafe(coords: String?): Point? {
        return try {
            val p = coords!!.split(",")
            Point(p[0].trim().toDouble(), p[1].trim().toDouble())
        } catch (e: Exception) {
            null
        }
    }

    private fun parseTimeSafe(str: String?): Long {
        return try {
            SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
                .parse(str ?: "")?.time ?: 0
        } catch (e: Exception) {
            0
        }
    }

    private fun formatTimeSafe(t: Long): String {
        return SimpleDateFormat("dd.MM HH:mm:ss", Locale.getDefault())
            .format(Date(if (t > 0) t else System.currentTimeMillis()))
    }
}