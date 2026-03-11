package com.example.arduinocarsecurity

import android.Manifest
import android.app.AlertDialog
import android.content.*
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.*
import android.telephony.SmsManager
import android.view.*
import android.widget.*
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import com.android.volley.Request
import com.android.volley.toolbox.StringRequest
import com.android.volley.toolbox.Volley
import java.text.SimpleDateFormat
import java.util.*

class ControlFragment : Fragment() {

    enum class ControlMode {
        HTTP,
        SMS
    }

    private lateinit var tvMode: TextView
    private val PREF_LAST_STATUS_TIME = "last_logged_status_time"
    private var controlMode = ControlMode.HTTP
    private var statusLoaded = false
    private lateinit var tvSystem: TextView
    private lateinit var tvConnection: TextView
    private lateinit var tvLastUpdate: TextView
    private lateinit var tvSmsStatus: TextView
    private var lastSystemState: Boolean? = null
    private lateinit var btnModeHttp: Button
    private lateinit var btnModeSms: Button
    private lateinit var btnOn: Button
    private lateinit var btnOff: Button
    private lateinit var btnLastAlarm: Button
    private lateinit var btnSmsStatus: Button
    private lateinit var btnExpand: TextView
    private lateinit var swipe: SwipeRefreshLayout
    private val handler = Handler(Looper.getMainLooper())
    private val SERVER_URL = "http://fedrum.atwebpages.com/status.txt"
    private val SET_URL = "http://fedrum.atwebpages.com/set.php"
    private val REQUEST_STATUS_URL =
        "http://fedrum.atwebpages.com/request_status.php"
    private val DEVICE_STATUS_URL =
        "http://fedrum.atwebpages.com/device_status.txt"
    private val PREFS = "car_security"
    private val PREF_LAST_ALARM = "last_alarm_time"
    private var httpOnline = false
    private var expanded = false
    private lateinit var smsReceiver: BroadcastReceiver

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_control, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        tvSystem = view.findViewById(R.id.tv_system)
        tvConnection = view.findViewById(R.id.tv_connection)
        tvLastUpdate = view.findViewById(R.id.tv_last_update)
        tvSmsStatus = view.findViewById(R.id.tv_sms_status)
        tvMode = view.findViewById(R.id.tv_mode)
        btnModeHttp = view.findViewById(R.id.btn_mode_http)
        btnModeSms = view.findViewById(R.id.btn_mode_sms)
        btnOn = view.findViewById(R.id.btn_on)
        btnOff = view.findViewById(R.id.btn_off)
        btnLastAlarm = view.findViewById(R.id.btn_last_alarm)
        btnSmsStatus = view.findViewById(R.id.btn_sms_status)
        btnExpand = view.findViewById(R.id.btn_expand)
        swipe = view.findViewById(R.id.swipe_refresh)

        loadMode()
        loadSavedState()
        setupModeButtons()
        setupControlButtons()
        setupRefresh()
        setupDropdown()
        initSmsReceiver()

        tvConnection.text = "⏳ Проверка сервера..."
        tvConnection.setTextColor(Color.GRAY)

        checkHttpStatus()
        startHttpPolling()
    }

    private fun setupModeButtons() {
        btnModeHttp.setOnClickListener {
            animateButton(it)
            controlMode = ControlMode.HTTP
            saveMode()
            updateModeUI()
        }

        btnModeSms.setOnClickListener {
            animateButton(it)
            controlMode = ControlMode.SMS
            saveMode()
            updateModeUI()
        }
        updateModeUI()
    }

    private fun updateModeUI() {
        if (controlMode == ControlMode.HTTP) {
            btnModeHttp.alpha = 1f
            btnModeSms.alpha = 0.5f
            tvMode.text = "Режим: HTTP"
        } else {
            btnModeHttp.alpha = 0.5f
            btnModeSms.alpha = 1f
            tvMode.text = "Режим: SMS"
        }
    }

    private fun animateButton(view: View) {
        view.animate()
            .scaleX(0.92f)
            .scaleY(0.92f)
            .setDuration(80)
            .withEndAction {
                view.animate()
                    .scaleX(1f)
                    .scaleY(1f)
                    .setDuration(80)
                    .start()

            }.start()
    }

    private fun saveMode() {
        val prefs =
            requireContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        prefs.edit()
            .putString("mode", controlMode.name)
            .apply()
    }
    private fun loadMode() {
        val prefs =
            requireContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val saved = prefs.getString("mode", "HTTP")
        controlMode = ControlMode.valueOf(saved!!)
    }

    private fun setupControlButtons() {
        btnOn.setOnClickListener {
            animateButton(it)
            sendCommand("ON")
        }
        btnOff.setOnClickListener {
            animateButton(it)
            sendCommand("OFF")
        }
        btnLastAlarm.setOnClickListener {
            animateButton(it)
            showLastAlarmDialog()
        }
        btnSmsStatus.setOnClickListener {
            animateButton(it)
            if (controlMode == ControlMode.SMS) {
                sendSms("STATUS")
            } else {
                requestHttpStatus()
            }
        }
    }

    private fun sendCommand(cmd: String) {
        when (controlMode) {
            ControlMode.HTTP -> sendHttp(cmd)
            ControlMode.SMS -> sendSms(cmd)
        }
    }

    private fun sendHttp(cmd: String, fail: () -> Unit = {}) {
        val queue = Volley.newRequestQueue(requireContext())
        val url = "$SET_URL?state=$cmd&t=${System.currentTimeMillis()}"
        val req = StringRequest(
            Request.Method.GET,
            url,
            {
                toast("HTTP команда отправлена")
                HistoryManager.addEvent(
                    requireContext(),
                    "COMMAND_HTTP",
                    "HTTP команда выполнена: $cmd"
                )
                if (cmd == "ON") updateSystem(true, true)
                if (cmd == "OFF") updateSystem(false, true)
            },
            {
                toast("HTTP ошибка")
                HistoryManager.addEvent(
                    requireContext(),
                    "SERVER",
                    "HTTP ошибка при команде $cmd"
                )
                fail()
            })
        queue.add(req)
    }

    private fun checkHttpStatus(force: Boolean = false) {
        val queue = Volley.newRequestQueue(requireContext())
        val url = "$SERVER_URL?t=${System.currentTimeMillis()}"
        val request = StringRequest(
            Request.Method.GET,
            url,
            { response ->
                httpOnline = true
                updateConnection(true)
                val clean = response.trim()
                if (clean.contains("ON"))
                    updateSystem(true)
                if (clean.contains("OFF"))
                    updateSystem(false)
                saveState(response)
                if (force)
                    toast("Статус обновлён")
            },
            {
                httpOnline = false
                updateConnection(false)
                if (force)
                    toast("Сервер недоступен")
            })

        queue.add(request)
    }

    private fun requestHttpStatus() {
        val queue = Volley.newRequestQueue(requireContext())
        val req = StringRequest(
            Request.Method.GET,
            REQUEST_STATUS_URL,
            {
                toast("Запрос отправлен")
                handler.postDelayed({
                    loadHttpStatus(attempt = 1)
                }, 12000)
            },
            {
                toast("Ошибка запроса")
            }
        )
        queue.add(req)
    }

    private fun loadHttpStatus(attempt: Int) {
        val queue = Volley.newRequestQueue(requireContext())
        val req = StringRequest(
            Request.Method.GET,
            DEVICE_STATUS_URL + "?t=" + System.currentTimeMillis(),
            { response ->
                if (response.isNotBlank()) {
                    val lastLine = response
                        .trim()
                        .lines()
                        .last()
                    updateSmsStatus(lastLine)
                } else if (attempt < 3) {
                    handler.postDelayed(
                        { loadHttpStatus(attempt + 1) },
                        3000
                    )
                } else {
                    toast("Статус не найден")
                }
            },
            {
                if (attempt < 3) {
                    handler.postDelayed(
                        { loadHttpStatus(attempt + 1) },
                        3000
                    )
                } else {
                    toast("Ошибка чтения статуса")
                }
            })
        queue.add(req)
    }

    private fun loadHttpStatus() {
        val queue = Volley.newRequestQueue(requireContext())
        val req = StringRequest(
            Request.Method.GET,
            DEVICE_STATUS_URL + "?t=" + System.currentTimeMillis(),
            { response ->
                if (response.isNotBlank()) {
                    val lastLine = response
                        .trim()
                        .lines()
                        .last()
                    updateSmsStatus(lastLine)
                }
            },
            {
                toast("Ошибка чтения статуса")
            })
        queue.add(req)
    }

    private fun startHttpPolling() {
        handler.post(object : Runnable {
            override fun run() {
                if (!isAdded) return
                checkHttpStatus()
                handler.postDelayed(this, 10000)
            }
        })
    }

    private fun sendSms(cmd: String) {
        if (!checkPermission()) return
        try {
            SmsManager.getDefault().sendTextMessage(
                SmsReceiver.DEVICE_NUMBER,
                null,
                cmd,
                null,
                null
            )
            toast("SMS отправлено")
            HistoryManager.addEvent(
                requireContext(),
                "COMMAND_SMS",
                "SMS отправлено: $cmd"
            )
        } catch (e: Exception) {

            toast("Ошибка SMS")
            HistoryManager.addEvent(
                requireContext(),
                "SYSTEM",
                "Ошибка отправки SMS"
            )
        }
    }

    private fun updateSystem(enabled: Boolean, log: Boolean = false) {
        if (enabled) {
            tvSystem.text = "🔐 Система включена"
            tvSystem.setTextColor(Color.parseColor("#2ECC71"))
        } else {
            tvSystem.text = "🔓 Система выключена"
            tvSystem.setTextColor(Color.parseColor("#E74C3C"))
        }
        tvLastUpdate.text = "Обновлено: ${time()}"
        if (lastSystemState == enabled) return
        lastSystemState = enabled
        if (log) {
            HistoryManager.addEvent(
                requireContext(),
                "SYSTEM",
                if (enabled) "Система включена"
                else "Система выключена"
            )
        }
    }

    private fun updateConnection(online: Boolean) {
        statusLoaded = true
        if (online) {
            tvConnection.text = "🟢 Сервер ONLINE"
            tvConnection.setTextColor(Color.parseColor("#2ECC71"))

        } else {
            tvConnection.text = "🔴 Сервер OFFLINE"
            tvConnection.setTextColor(Color.parseColor("#E74C3C"))

        }
    }

    private fun initSmsReceiver() {
        smsReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                val msg = intent?.getStringExtra("message") ?: return
                updateSmsStatus(msg)

            }
        }
    }

    private fun updateSmsStatus(text: String) {
        val parts = text.trim().split("|")
        if (parts.size >= 7) {
            val statusTime = parts[6]
            val prefs = requireContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            val savedTime = prefs.getString(PREF_LAST_STATUS_TIME, null)
            if (statusTime != savedTime) {
                HistoryManager.addEvent(
                    requireContext(),
                    "RESPONSE_SMS",
                    parseDeviceStatus(text)
                )
                prefs.edit()
                    .putString(PREF_LAST_STATUS_TIME, statusTime)
                    .apply()
            }
        }
        val parsed = parseDeviceStatus(text)
        tvSmsStatus.text = parsed
        tvSmsStatus.visibility = View.VISIBLE
        expanded = true
        btnExpand.text = "▲ Детали"
        saveState(text)
        if (text.contains("System: ON"))
            updateSystem(true, true)
        if (text.contains("System: OFF"))
            updateSystem(false, true)
        tvLastUpdate.text = "Статус получен: ${time()}"
    }

    private fun setupDropdown() {
        btnExpand.setOnClickListener {
            expanded = !expanded
            if (expanded) {
                tvSmsStatus.visibility = View.VISIBLE
                btnExpand.text = "▲ Скрыть"
            } else {
                tvSmsStatus.visibility = View.GONE
                btnExpand.text = "▼ Детали SMS"

            }
            saveExpanded()
        }
    }

    private fun setupRefresh() {
        swipe.setOnRefreshListener {
            checkHttpStatus(true)
            handler.postDelayed({
                if (isAdded)
                    swipe.isRefreshing = false

            }, 600)
        }
    }

    private fun saveState(text: String) {
        val prefs =
            requireContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        prefs.edit()
            .putString("last_status", text)
            .putLong("last_time", System.currentTimeMillis())
            .apply()
    }

    private fun loadSavedState() {
        val prefs =
            requireContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val status = prefs.getString("last_status", null)
        val time = prefs.getLong("last_time", 0)
        expanded = prefs.getBoolean("expanded", false)
        tvSmsStatus.visibility = View.VISIBLE
        expanded = true
        btnExpand.text = "▲ Скрыть"
        status?.let {
            tvSmsStatus.text = parseDeviceStatus(it)
            if (expanded) {
                tvSmsStatus.visibility = View.VISIBLE
                btnExpand.text = "▲ Скрыть"
            } else {
                tvSmsStatus.visibility = View.GONE
                btnExpand.text = "▼ Детали"
            }
            if (it.startsWith("1"))
                updateSystem(true)

            if (it.startsWith("0"))
                updateSystem(false)
        }
        if (time > 0)
            tvLastUpdate.text = "Последний статус: ${formatTime(time)}"
    }
    private fun checkPermission(): Boolean {
        if (ContextCompat.checkSelfPermission(
                requireContext(),
                Manifest.permission.SEND_SMS
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            ActivityCompat.requestPermissions(
                requireActivity(),
                arrayOf(
                    Manifest.permission.SEND_SMS,
                    Manifest.permission.RECEIVE_SMS
                ),
                100
            )
            return false
        }
        return true
    }

    private fun toast(text: String) {
        Toast.makeText(context, text, Toast.LENGTH_SHORT).show()
    }

    private fun time(): String {
        return SimpleDateFormat(
            "HH:mm:ss",
            Locale.getDefault()
        ).format(Date())
    }

    private fun formatTime(t: Long): String {
        return SimpleDateFormat(
            "dd.MM HH:mm:ss",
            Locale.getDefault()
        ).format(Date(t))
    }

    private fun saveExpanded() {
        val prefs =
            requireContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        prefs.edit()
            .putBoolean("expanded", expanded)
            .apply()
    }

    override fun onResume() {
        super.onResume()
        ContextCompat.registerReceiver(
            requireContext(),
            smsReceiver,
            IntentFilter(SmsReceiver.SMS_ACTION),
            ContextCompat.RECEIVER_NOT_EXPORTED
        )
        loadHttpStatus()
    }

    override fun onPause() {
        super.onPause()
        requireContext().unregisterReceiver(smsReceiver)
    }

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacksAndMessages(null)
    }

    fun reloadFromStorage() {
        if (!isAdded) return
        val prefs =
            requireContext().getSharedPreferences("car_security", Context.MODE_PRIVATE)
        val status = prefs.getString("last_status", null)
        val time = prefs.getLong("last_time", 0)
        status?.let {
            tvSmsStatus.text = parseDeviceStatus(it)
            if (it.contains("ON"))
                updateSystem(true)
            if (it.contains("OFF"))
                updateSystem(false)
        }
        if (time > 0)
            tvLastUpdate.text = "Последнее: ${formatTime(time)}"
    }

    private fun parseDeviceStatus(raw: String): String {
        try {
            val parts = raw.trim().split("|")
            if (parts.size < 7) return raw
            val system =
                if (parts[0] == "1") "🔐 ВКЛЮЧЕНА"
                else "🔓 ВЫКЛЮЧЕНА"
            val alarm =
                if (parts[1] == "1") "🚨 ТРЕВОГА"
                else "✔ Нет"
            val mic =
                if (parts[2] == "0")
                    "🔇 Тишина"
                else
                    "🔊 Шум"
            val gyro = parts[3]
            val gps = parts[5]
            val time = parts[6]
            return """
🔐 Система: $system
🚨 Тревога: $alarm
🎤 Микрофон: $mic
🧭 Гироскоп: $gyro
📍 GPS: $gps
⏱ Последнее обновление: $time
        """.trimIndent()
        } catch (e: Exception) {
            return raw
        }
    }

    private fun showLastAlarmDialog() {
        val queue = Volley.newRequestQueue(requireContext())
        val url = "http://fedrum.atwebpages.com/alarm_log.txt?t=${System.currentTimeMillis()}"
        val req = StringRequest(
            Request.Method.GET,
            url,
            { response ->
                val last = response.trim().lines().lastOrNull()
                if (last == null) {
                    toast("Нет данных тревоги")
                    return@StringRequest
                }
                val parts = last.split("|").map { it.trim() }
                if (parts.size < 4) {
                    toast("Ошибка формата тревоги")
                    return@StringRequest
                }
                val time = parts[0]
                val sensor = parts[2]
                val coords = parts[3]
                val prefs = requireContext()
                    .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                val lastSaved = prefs.getString(PREF_LAST_ALARM, null)
                if (time != lastSaved) {
                    HistoryManager.logAlarm(
                        requireContext(),
                        "Сработал сенсор $sensor ($coords)"
                    )
                    prefs.edit()
                        .putString(PREF_LAST_ALARM, time)
                        .apply()
                }
                val coordParts = coords.split(",")
                if (coordParts.size < 2) {
                    toast("Ошибка координат")
                    return@StringRequest
                }
                val lat = coordParts[0].trim().toDoubleOrNull()
                val lon = coordParts[1].trim().toDoubleOrNull()
                if (lat == null || lon == null) {
                    toast("Ошибка координат")
                    return@StringRequest
                }
                val dialogView = layoutInflater.inflate(
                    R.layout.dialog_alarm,
                    null
                )
                val mapView = dialogView.findViewById<com.yandex.mapkit.mapview.MapView>(R.id.alarm_map)
                val tvTime = dialogView.findViewById<TextView>(R.id.tv_alarm_time)
                val tvSensor = dialogView.findViewById<TextView>(R.id.tv_alarm_sensor)
                val tvCoords = dialogView.findViewById<TextView>(R.id.tv_alarm_coords)
                tvTime.text = "🕒 $time"
                tvSensor.text = "🎤 Сенсор: $sensor"
                tvCoords.text = "📍 $coords"
                val point = com.yandex.mapkit.geometry.Point(lat, lon)
                mapView.map.move(
                    com.yandex.mapkit.map.CameraPosition(point, 16f, 0f, 0f)
                )
                mapView.map.mapObjects.addPlacemark(point)
                val dialog = AlertDialog.Builder(requireContext())
                    .setView(dialogView)
                    .setPositiveButton("OK") { d, _ ->
                        mapView.onStop()
                        d.dismiss()
                    }
                    .create()
                dialog.show()
                mapView.onStart()
            },
            {
                toast("Ошибка загрузки тревоги")
            }
        )
        queue.add(req)
    }
}