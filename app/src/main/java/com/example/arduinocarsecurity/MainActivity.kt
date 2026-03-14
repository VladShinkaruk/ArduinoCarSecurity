package com.example.arduinocarsecurity

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.google.android.material.bottomnavigation.BottomNavigationView

class MainActivity : AppCompatActivity() {
    private lateinit var bottomNav: BottomNavigationView
    private val controlFragment = ControlFragment()
    private val mapFragment = MapFragment()
    private val historyFragment = HistoryFragment()
    private var activeFragment: Fragment? = null
    private val smsUpdateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val fragment = activeFragment
            if (fragment is ControlFragment) {
                fragment.reloadFromStorage()
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        bottomNav = findViewById(R.id.bottom_nav)

        if (savedInstanceState == null) {
            supportFragmentManager.beginTransaction()
                .add(R.id.fragment_container, historyFragment, "history")
                .hide(historyFragment)
                .commit()
            supportFragmentManager.beginTransaction()
                .add(R.id.fragment_container, mapFragment, "map")
                .hide(mapFragment)
                .commit()
            supportFragmentManager.beginTransaction()
                .add(R.id.fragment_container, controlFragment, "control")
                .commit()

            activeFragment = controlFragment
        }

        bottomNav.setOnItemSelectedListener { item ->
            when (item.itemId) {
                R.id.nav_control -> switchFragment(controlFragment)
                R.id.nav_map -> switchFragment(mapFragment)
                R.id.nav_history -> switchFragment(historyFragment)
                else -> false
            }
        }
    }

    private fun switchFragment(target: Fragment): Boolean {
        if (activeFragment == target) return false
        supportFragmentManager.beginTransaction()
            .setReorderingAllowed(true)
            .setCustomAnimations(
                R.anim.fragment_fade_in,
                0
            )
            .hide(activeFragment!!)
            .show(target)
            .commitAllowingStateLoss()
        activeFragment = target
        return true
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
}