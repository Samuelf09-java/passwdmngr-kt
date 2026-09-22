package com.samuelf09.passwdmngr.viewmodel

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.samuelf09.passwdmngr.Native
import com.samuelf09.passwdmngr.PasswdEntry
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class MainViewModel : ViewModel() {

    var selectedEntryId by mutableStateOf<Int?>(null)
    private set

    var sidebarCollapsed by mutableStateOf(true)
    private set

    var entries: Array<PasswdEntry> = Native.storageGetEntries() ?: emptyArray()

    sealed class NavigationEvent {
        data object ToAddEntry: NavigationEvent()
        data object ToLogin : NavigationEvent()
    }

    private val _navigation = MutableStateFlow<NavigationEvent?>(null)
    val navigation = _navigation.asStateFlow()
    fun clearNavigation() {
        _navigation.value = null
    }

    fun reloadEntries() {
        entries = Native.storageGetEntries() ?: emptyArray()
    }

    fun selectEntry(id: Int?) {
        selectedEntryId = id
    }

    fun toggleSidebar() {
        sidebarCollapsed = !sidebarCollapsed
    }

    fun addEntry() {
        _navigation.value = NavigationEvent.ToAddEntry
    }
}
