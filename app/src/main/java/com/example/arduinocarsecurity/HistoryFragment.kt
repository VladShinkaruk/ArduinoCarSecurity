package com.example.arduinocarsecurity

import android.os.Bundle
import android.view.*
import android.widget.*
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout

class HistoryFragment : Fragment() {

    private lateinit var recycler: RecyclerView
    private lateinit var swipe: SwipeRefreshLayout
    private lateinit var spinner: Spinner
    private lateinit var adapter: HistoryAdapter
    private var history: List<HistoryItem> = listOf()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_history, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {

        recycler = view.findViewById(R.id.recycler_history)
        swipe = view.findViewById(R.id.swipe_refresh_history)
        spinner = view.findViewById(R.id.spinner_sort)
        recycler.layoutManager = LinearLayoutManager(requireContext())
        adapter = HistoryAdapter(emptyList())
        recycler.adapter = adapter

        setupSpinner()
        loadHistory()

        swipe.setOnRefreshListener {
            loadHistory()
            swipe.isRefreshing = false
        }
    }

    private fun setupSpinner() {
        val options = listOf(
            "Новые события",
            "Старые события",
            "Команды",
            "Ответы",
            "Тревоги",
            "Система"
        )
        val spinnerAdapter = ArrayAdapter(
            requireContext(),
            android.R.layout.simple_spinner_dropdown_item,
            options
        )
        spinner.adapter = spinnerAdapter
        spinner.onItemSelectedListener =
            object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(
                    parent: AdapterView<*>,
                    view: View?,
                    position: Int,
                    id: Long
                ) {
                    val filtered = when (position) {
                        0 -> history.sortedByDescending { it.time }
                        1 -> history.sortedBy { it.time }
                        2 -> history.filter { it.type.startsWith("COMMAND") }
                        3 -> history.filter { it.type.startsWith("RESPONSE") }
                        4 -> history.filter { it.type.startsWith("ALARM") }
                        5 -> history.filter { it.type.startsWith("SYSTEM") }
                        else -> history
                    }
                    recycler.adapter = HistoryAdapter(filtered)
                }
                override fun onNothingSelected(parent: AdapterView<*>) {}
            }
    }

    private fun loadHistory() {
        history = HistoryManager
            .getHistory(requireContext())
            .sortedByDescending { it.time }
        recycler.adapter = HistoryAdapter(history)
    }

    override fun onResume() {
        super.onResume()
        loadHistory()
    }
}