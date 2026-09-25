package com.samuelf09.passwdmngr.viewmodel

import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.samuelf09.passwdmngr.Native
import com.samuelf09.passwdmngr.emptyPasswdEntry
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class LoginViewModel : ViewModel() {

    var username by mutableStateOf("")
        private set

    var password by mutableStateOf("")
        private set

    var errorMessage by mutableStateOf<String?>(null)
        private set

    private fun isValidInput(input: String) = input.length in 4..32

    sealed class NavigationEvent {
        data object ToMain: NavigationEvent()
        data object ToCreateAccount : NavigationEvent()
    }

    private val _navigation = MutableStateFlow<NavigationEvent?>(null)
    val navigation = _navigation.asStateFlow()
    fun clearNavigation() {
        _navigation.value = null
    }

    fun onUsernameChanged(newValue: String) {
        username = newValue
    }

    fun onPasswordChanged(newValue: String) {
        password = newValue
    }

    fun onLoginClicked() {
        if (!Native.login(username, password)) {
            errorMessage = Native.getError()
            return
        }
        _navigation.value = NavigationEvent.ToMain
    }

    fun onCreateAccountClicked() {
        _navigation.value = NavigationEvent.ToCreateAccount
    }
}