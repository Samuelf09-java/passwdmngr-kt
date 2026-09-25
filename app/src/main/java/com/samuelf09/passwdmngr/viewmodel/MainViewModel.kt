package com.samuelf09.passwdmngr.viewmodel

import android.app.Application
import android.view.View
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import com.samuelf09.passwdmngr.Native
import com.samuelf09.passwdmngr.PasswdEntry
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class MainViewModel(application: Application) : AndroidViewModel(application) {

    var selectedEntryId by mutableStateOf<Int?>(null)
    private set

    var sidebarCollapsed by mutableStateOf(true)
    private set

    var entries: Array<PasswdEntry> = Native.storageGetEntries() ?: emptyArray()

    private val _licenseText = MutableStateFlow<String?>(null)
    val licenseText = _licenseText

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

    // Menu callbacks

    fun onLogoutClicked() {
        Native.logout()
        _navigation.value = NavigationEvent.ToLogin
    }

    fun onExportClicked() {

    }

    fun onImportClicked() {

    }

    fun onBackupClicked() {

    }

    fun onRestoreBackupClicked() {

    }

    fun onViewLogClicked() {

    }

    fun onClearLogClicked() {

    }

    fun onChangePasswordClicked() {

    }

    fun onDeleteAccountClicked() {

    }

    fun onAboutClicked() {
        _licenseText.value = getApplication<Application>().assets.open("LICENSE.txt")
            .bufferedReader()
            .use { it.readText() }

    }

    fun onAboutDestroyed() {
        _licenseText.value = null
    }

}
