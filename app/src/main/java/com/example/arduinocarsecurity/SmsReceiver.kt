package com.example.arduinocarsecurity

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.provider.Telephony

class SmsReceiver : BroadcastReceiver() {
    companion object {
        const val DEVICE_NUMBER = "+375296625470"
        const val SMS_ACTION = "SMS_FROM_DEVICE"
        private const val PREFS = "car_security"
    }

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != "android.provider.Telephony.SMS_RECEIVED") return
        for (sms in Telephony.Sms.Intents.getMessagesFromIntent(intent)) {
            val sender = sms.originatingAddress ?: continue
            val body = sms.messageBody ?: continue
            val cleanSender = sender.replace(" ", "")
            if (!cleanSender.contains(DEVICE_NUMBER)) continue
            if (!body.contains("System:") && !body.contains("TREVOGA")) continue
            val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            prefs.edit()
                .putString("last_status", body)
                .putLong("last_update_time", System.currentTimeMillis())
                .apply()
            val isAlarm = body.contains("TREVOGA")
            if (isAlarm) {
                val sensor = body.substringAfter("Sensor:", "")
                    .substringBefore(",")
                    .trim()
                HistoryManager.addEvent(
                    context,
                    "ALARM",
                    "🚨 ТРЕВОГА! Датчик: $sensor"
                )
            } else {
                val status = body.substringAfter("Alert:", "")
                    .substringBefore("\n")
                    .trim()
                HistoryManager.addEvent(
                    context,
                    "RESPONSE",
                    "Получено SMS от устройства (Статус: $status)"
                )
            }
            val updateIntent = Intent(SMS_ACTION)
            updateIntent.putExtra("message", body)
            context.sendBroadcast(updateIntent)
        }
    }
}