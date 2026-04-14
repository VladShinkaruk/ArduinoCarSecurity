package com.example.arduinocarsecurity

import android.content.Context
import android.content.SharedPreferences
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.*
import org.mockito.junit.MockitoJUnitRunner

@RunWith(MockitoJUnitRunner::class)
class HistoryManagerTest {
    @Mock private lateinit var mockContext: Context
    @Mock private lateinit var mockPrefs: SharedPreferences
    @Mock private lateinit var mockEditor: SharedPreferences.Editor

    @Before
    fun setup() {
        `when`(mockContext.getSharedPreferences("car_security", Context.MODE_PRIVATE))
            .thenReturn(mockPrefs)
        `when`(mockPrefs.edit()).thenReturn(mockEditor)
        `when`(mockEditor.putString(anyString(), anyString())).thenReturn(mockEditor)
    }

    @Test
    fun `addEvent сохраняет событие в SharedPreferences`() {
        HistoryManager.addEvent(mockContext, "COMMAND_HTTP", "Тестовая команда")
        verify(mockPrefs).edit()
        verify(mockEditor).apply()
        verify(mockEditor).putString(eq("history"), anyString())
    }

    @Test
    fun `getHistory возвращает пустой список когда данных нет`() {
        `when`(mockPrefs.getString("history", null)).thenReturn(null)
        val result = HistoryManager.getHistory(mockContext)
        assertTrue(result.isEmpty())
    }

    @Test
    fun `getHistory парсит JSON и возвращает список`() {
        val fakeJson = """[{"time":1234567890,"type":"TEST","message":"Hello"}]"""
        `when`(mockPrefs.getString("history", null)).thenReturn(fakeJson)
        val result = HistoryManager.getHistory(mockContext)
        assertEquals(1, result.size)
        assertEquals("TEST", result[0].type)
        assertEquals("Hello", result[0].message)
        assertEquals(1234567890L, result[0].time)
    }

    @Test
    fun `addEvent обрезает историю до 100 последних записей`() {
        repeat(105) { i ->
            HistoryManager.addEvent(mockContext, "TEST", "Event $i")
        }
        verify(mockEditor, atLeast(100)).apply()
    }
}