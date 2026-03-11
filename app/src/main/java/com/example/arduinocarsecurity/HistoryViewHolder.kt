package com.example.arduinocarsecurity

import android.graphics.Color
import android.view.View
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import java.text.SimpleDateFormat
import java.util.*

class HistoryViewHolder(view: View) : RecyclerView.ViewHolder(view) {
    private val tvType: TextView = view.findViewById(R.id.tv_type)
    private val tvMessage: TextView = view.findViewById(R.id.tv_message)
    private val tvTime: TextView = view.findViewById(R.id.tv_time)
    fun bind(item: HistoryItem) {
        val (title, color) = when {
            item.type.startsWith("COMMAND") ->
                "📤 Команда" to Color.parseColor("#1565C0")
            item.type.startsWith("RESPONSE") ->
                "📩 Ответ" to Color.parseColor("#2E7D32")
            item.type.startsWith("ALARM") ->
                "🚨 Тревога" to Color.RED
            item.type.startsWith("SYSTEM") ->
                "⚙ Система" to Color.parseColor("#6A1B9A")
            item.type.startsWith("GPS") ->
                "📍 GPS" to Color.parseColor("#00897B")
            item.type.startsWith("SERVER") ->
                "🌐 Сервер" to Color.parseColor("#455A64")
            else ->
                item.type to Color.BLACK
        }
        tvType.text = title
        tvType.setTextColor(color)
        tvMessage.text = item.message
        val sdf = SimpleDateFormat("dd.MM HH:mm:ss", Locale.getDefault())
        tvTime.text = sdf.format(Date(item.time))
    }
}