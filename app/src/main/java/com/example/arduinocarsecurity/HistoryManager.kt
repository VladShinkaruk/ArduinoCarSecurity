package com.example.arduinocarsecurity

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

object HistoryManager {

    private const val PREFS = "car_security"
    private const val KEY = "history"

    fun addEvent(context: Context, type: String, message: String) {
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val json = prefs.getString(KEY, null)
        val array = if (json != null) JSONArray(json) else JSONArray()
        val obj = JSONObject()
        obj.put("time", System.currentTimeMillis())
        obj.put("type", type)
        obj.put("message", message)
        array.put(obj)

        val trimmed = JSONArray()
        val start = if (array.length() > 100) array.length() - 100 else 0
        for (i in start until array.length()) {
            trimmed.put(array.getJSONObject(i))
        }
        prefs.edit().putString(KEY, trimmed.toString()).apply()
    }

    fun getHistory(context: Context): List<HistoryItem> {
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val json = prefs.getString(KEY, null) ?: return emptyList()
        val array = JSONArray(json)
        val list = mutableListOf<HistoryItem>()
        for (i in 0 until array.length()) {
            val obj = array.getJSONObject(i)
            list.add(
                HistoryItem(
                    obj.getLong("time"),
                    obj.getString("type"),
                    obj.getString("message")
                )
            )
        }
        return list
    }

    fun logAlarm(context: Context, text: String) {
        addEvent(context, "ALARM", text)
    }
}