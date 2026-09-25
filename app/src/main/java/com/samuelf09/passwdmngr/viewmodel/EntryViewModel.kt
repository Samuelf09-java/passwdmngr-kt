package com.samuelf09.passwdmngr.viewmodel

import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.samuelf09.passwdmngr.Native
import com.samuelf09.passwdmngr.PasswdEntry
import com.samuelf09.passwdmngr.emptyPasswdEntry
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class EntryViewModel: ViewModel() {

    var entry by mutableStateOf<PasswdEntry?>(null)
        private set

    var showDeleteDialog by mutableStateOf(false)
    var errorMessage by mutableStateOf<String?>(null)

    sealed class NavigationEvent {
        data object ToEdit: NavigationEvent()
        data object ToMain: NavigationEvent()
    }

    private val _navigation = MutableStateFlow<NavigationEvent?>(null)
    val navigation = _navigation.asStateFlow()
    fun clearNavigation() {
        _navigation.value = null
    }

    fun init(id: Int) {
        entry = Native.storageGetEntry(id) ?: emptyPasswdEntry()
        if (entry!!.id < 0) {
            throw IllegalStateException("Invalid entry id selected!")
        }
    }

    fun onEditClicked() {
        _navigation.value = NavigationEvent.ToEdit
    }

    fun onDeleteClicked() {
        showDeleteDialog = true
    }

    fun deleteEntry() {
        if (!Native.deleteEntry(entry!!.id)) {
            errorMessage = Native.getError()
            return
        }
        _navigation.value = NavigationEvent.ToMain
    }
}