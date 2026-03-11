package com.example.arduinocarsecurity

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.google.android.material.bottomnavigation.BottomNavigationView

class MainActivity : AppCompatActivity() {
    private lateinit var bottomNav: BottomNavigationView
    private var currentTab = R.id.nav_control
    private var navLocked = true
    private var lastClickTime = 0L
    private val unlockHandler = Handler(Looper.getMainLooper())
    private val unlockRunnable = Runnable { navLocked = false }

    private val smsUpdateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val fragment = supportFragmentManager
                .findFragmentById(R.id.fragment_container)
            if (fragment is ControlFragment) {
                fragment.reloadFromStorage()
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        bottomNav = findViewById(R.id.bottom_nav)
        loadFragment(ControlFragment())

        navLocked = true
        unlockHandler.postDelayed(unlockRunnable, 4000)
        bottomNav.setOnItemSelectedListener { item ->
            val now = System.currentTimeMillis()
            if (navLocked) {
                return@setOnItemSelectedListener false
            }
            if (now - lastClickTime < 600) {
                return@setOnItemSelectedListener false
            }
            if (item.itemId == currentTab) {
                return@setOnItemSelectedListener false
            }
            lastClickTime = now
            currentTab = item.itemId
            when (item.itemId) {
                R.id.nav_control -> loadFragment(ControlFragment())
                R.id.nav_map -> loadFragment(MapFragment())
                R.id.nav_history -> loadFragment(HistoryFragment())
            }
            true
        }
    }

    override fun onResume() {
        super.onResume()
        ContextCompat.registerReceiver(
            this,
            smsUpdateReceiver,
            IntentFilter(SmsReceiver.SMS_ACTION),
            ContextCompat.RECEIVER_NOT_EXPORTED
        )
    }

    override fun onPause() {
        super.onPause()
        unregisterReceiver(smsUpdateReceiver)
    }

    override fun onDestroy() {
        super.onDestroy()
        unlockHandler.removeCallbacks(unlockRunnable)
    }

    private fun loadFragment(fragment: Fragment) {
        supportFragmentManager.beginTransaction()
            .setCustomAnimations(
                android.R.anim.fade_in,
                android.R.anim.fade_out
            )
            .replace(R.id.fragment_container, fragment)
            .commit()
    }
}