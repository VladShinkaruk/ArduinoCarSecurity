package com.example.arduinocarsecurity

import android.app.Application
import androidx.appcompat.app.AppCompatDelegate
import com.yandex.mapkit.MapKitFactory

class App : Application() {
    override fun onCreate() {
        super.onCreate()
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO)

        MapKitFactory.setApiKey("51f4b088-f868-4ffd-ab71-7f1a8b8beb28")
        MapKitFactory.initialize(this)
    }
}