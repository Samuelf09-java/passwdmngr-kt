package com.samuelf09.passwdmngr.viewmodel

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.samuelf09.passwdmngr.Native
import com.samuelf09.passwdmngr.PasswdEntry

class MainViewModel : ViewModel() {

    var selectedEntryId by mutableStateOf<Int?>(null)
    private set

    var sidebarCollapsed by mutableStateOf(true)
    private set

    val entries: Array<PasswdEntry> = Native.storageGetEntries() ?: emptyArray()

    fun selectEntry(id: Int) {
        selectedEntryId = id
    }

    fun toggleSidebar() {
        sidebarCollapsed = !sidebarCollapsed
    }
}
