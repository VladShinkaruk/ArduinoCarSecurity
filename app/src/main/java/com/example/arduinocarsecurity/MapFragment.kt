package com.example.arduinocarsecurity

import android.os.Bundle
import android.view.*
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.yandex.mapkit.MapKitFactory
import com.yandex.mapkit.geometry.Point
import com.yandex.mapkit.map.CameraPosition
import com.yandex.mapkit.mapview.MapView
import com.google.android.material.floatingactionbutton.FloatingActionButton
import com.android.volley.toolbox.StringRequest
import com.android.volley.toolbox.Volley
import java.text.SimpleDateFormat
import java.util.*
import android.graphics.Bitmap
import android.graphics.Canvas
import com.yandex.runtime.image.ImageProvider
import android.graphics.PointF
import com.yandex.mapkit.map.IconStyle
import com.yandex.mapkit.Animation

class MapFragment : Fragment() {
    private lateinit var mapView: MapView
    private lateinit var tvLocation: TextView
    private lateinit var tvTimestamp: TextView
    private lateinit var tvStatus: TextView
    private lateinit var btnRefresh: FloatingActionButton
    private val prefsName = "car_security"
    private val deviceUrl =
        "http://fedrum.atwebpages.com/device_status.txt"
    private val alarmUrl =
        "http://fedrum.atwebpages.com/alarm_log.txt"
    private var lastPoint: Point? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_map, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        mapView = view.findViewById(R.id.map_view)
        tvLocation = view.findViewById(R.id.tv_location)
        tvTimestamp = view.findViewById(R.id.tv_timestamp)
        tvStatus = view.findViewById(R.id.tv_status)
        btnRefresh = view.findViewById(R.id.btn_refresh)
        moveToDefaultLocation()
        btnRefresh.setOnClickListener {
            loadBestLocation()
        }
        loadBestLocation()
    }

    override fun onStart() {
        super.onStart()
        MapKitFactory.getInstance().onStart()
        mapView.onStart()

    }

    override fun onStop() {
        mapView.onStop()
        MapKitFactory.getInstance().onStop()
        super.onStop()
    }

    private fun loadBestLocation() {
        loadDeviceStatus { device ->
            loadAlarmLocation { alarm ->
                val best = when {
                    device == null -> alarm
                    alarm == null -> device
                    device.time > alarm.time -> device
                    else -> alarm
                }
                best?.let {
                    showLocation(it.point, it.time, it.source)
                }
            }
        }
    }

    private fun loadDeviceStatus(callback: (LocationData?) -> Unit) {
        val queue = Volley.newRequestQueue(requireContext())
        val req = StringRequest(deviceUrl,
            { text ->
                val line = text.trim().lines().lastOrNull()
                if (line == null) {
                    callback(null)
                    return@StringRequest
                }
                val parts = line.split("|")
                if (parts.size < 7) {
                    callback(null)
                    return@StringRequest
                }
                val coords = parts[5]
                val timeStr = parts[6]
                val point = parseCoords(coords)
                val time = parseTime(timeStr)
                if (point != null)
                    callback(LocationData(point, time, "DEVICE"))
                else
                    callback(null)
            },
            {
                callback(null)
            })
        queue.add(req)
    }

    private fun loadAlarmLocation(callback: (LocationData?) -> Unit) {
        val queue = Volley.newRequestQueue(requireContext())
        val req = StringRequest(alarmUrl,
            { text ->
                val lines = text.trim().split("\n")
                val last = lines.lastOrNull() ?: return@StringRequest
                val parts = last.split("|")
                if (parts.size < 3) {
                    callback(null)
                    return@StringRequest
                }
                val timeStr = parts[0].trim()
                val coords = parts[3].trim()
                val point = parseCoords(coords)
                val time = parseTime(timeStr)
                if (point != null) {
                    callback(LocationData(point, time, "SERVER"))
                } else {
                    callback(null)
                }
            },
            {
                callback(null)

            })
        queue.add(req)
    }

    private fun showLocation(point: Point, time: Long, source: String) {
        lastPoint = point
        val map = mapView.map
        map.mapObjects.clear()
        val placemark = map.mapObjects.addPlacemark().apply {
            geometry = point
            setIcon(
                getPinIcon(),
                IconStyle().apply {
                    scale = 1.1f
                    anchor = PointF(0.5f, 1f)
                }
            )
        }
        map.move(
            CameraPosition(point, 17f, 0f, 0f),
            Animation(Animation.Type.SMOOTH, 1.5f),
            null
        )
        tvLocation.text =
            "📍 ${point.latitude}, ${point.longitude}"
        tvTimestamp.text =
            "Обновлено: ${formatTime(time)}"
        tvStatus.text =
            "Источник: $source"
    }

    private fun parseCoords(coords: String?): Point? {
        return try {
            val p = coords!!.split(",")
            Point(
                p[0].trim().toDouble(),
                p[1].trim().toDouble()
            )
        } catch (e: Exception) {
            null
        }
    }

    private fun getPinIcon(): ImageProvider {
        val drawable = requireContext()
            .getDrawable(R.drawable.ic_map_pin)!!
            .mutate()
        val bitmap = Bitmap.createBitmap(
            drawable.intrinsicWidth,
            drawable.intrinsicHeight,
            Bitmap.Config.ARGB_8888
        )
        val canvas = Canvas(bitmap)
        drawable.setBounds(
            0,
            0,
            canvas.width,
            canvas.height
        )
        drawable.draw(canvas)
        return ImageProvider.fromBitmap(bitmap)
    }

    private fun parseTime(str: String): Long {
        return try {
            SimpleDateFormat(
                "yyyy-MM-dd HH:mm:ss",
                Locale.getDefault()
            ).parse(str)?.time ?: 0
        } catch (e: Exception) {
            0
        }
    }

    private fun formatTime(t: Long): String {
        return SimpleDateFormat(
            "dd.MM HH:mm:ss",
            Locale.getDefault()
        ).format(Date(t))
    }

    private fun moveToDefaultLocation() {
        val defaultPoint = Point(53.9, 27.56)
        mapView.map.move(
            CameraPosition(defaultPoint, 5f, 0f, 0f),
            Animation(Animation.Type.SMOOTH, 1f),
            null
        )
    }

    data class LocationData(
        val point: Point,
        val time: Long,
        val source: String
    )
}