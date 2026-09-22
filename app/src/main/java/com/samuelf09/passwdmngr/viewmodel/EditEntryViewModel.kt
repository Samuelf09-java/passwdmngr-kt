package com.samuelf09.passwdmngr.viewmodel

import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.samuelf09.passwdmngr.Native
import com.samuelf09.passwdmngr.PasswdEntry
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class EditEntryViewModel : ViewModel() {

    var add = true // default to new entry
        private set
    var id = -1
        private set
    var service by mutableStateOf("")
    var username by mutableStateOf("")
    var password by mutableStateOf("")
    var notes by mutableStateOf("")

    var errorMessage by mutableStateOf<String?>(null)

    sealed class NavigationEvent {
        data object ToMain: NavigationEvent()
        data object ToMainWithId: NavigationEvent()
    }

    private val _navigation = MutableStateFlow<NavigationEvent?>(null)
    val navigation = _navigation.asStateFlow()
    fun clearNavigation() {
        _navigation.value = null
    }

    fun init(entry: PasswdEntry) {
        id = if (entry.id < 0) Native.storageGetNextId() else entry.id
        service = entry.service ?: ""
        username = entry.username ?: ""
        password = entry.password ?: ""
        notes = entry.notes ?: ""
        if (entry.id > 0) add = false // updating existing entry
    }

    fun onServiceChanged(newVal: String) {
        service = newVal
    }

    fun onUsernameChanged(newVal: String) {
        username = newVal
    }

    fun onPasswordChanged(newVal: String) {
        password = newVal
    }

    fun onNotesChanged(newVal: String) {
        notes = newVal
    }

    fun onSaveClicked() {

        if (service.isBlank() || service.length > 128 || username.length > 128 || password.length > 128 || notes.length > 1024) {
            errorMessage = "Service cannot be blank & fields must be 128 characters max (1024 for notes)!"
            return
        }

        val saveEntry = PasswdEntry(id, service, username, password, notes)
        if (add && Native.isDuplicateEntry(saveEntry)) {
            errorMessage = "Duplicate service!"
            return
        }

        if (add) {
            if (!Native.addEntry(saveEntry)) {
                errorMessage = "Failed to add entry! (internal error)"
                return
            }
            Log.d("passwdmngr", "Created entry $saveEntry")
            _navigation.value = NavigationEvent.ToMain
            return
        }

        if (!Native.updateEntry(id, saveEntry)) {
            errorMessage = "Failed to save edits! (internal error)"
            return
        }
        _navigation.value = NavigationEvent.ToMainWithId
    }

    fun onCancelClicked() {
        _navigation.value = if (add) NavigationEvent.ToMain else NavigationEvent.ToMainWithId
    }
}