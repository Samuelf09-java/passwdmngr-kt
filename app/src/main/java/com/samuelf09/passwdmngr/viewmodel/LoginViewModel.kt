package com.samuelf09.passwdmngr.viewmodel

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.samuelf09.passwdmngr.Native
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class LoginViewModel : ViewModel() {

    val maxUnameLength = 32
    val maxPasswdLength = 32

    var username by mutableStateOf("")
        private set

    var password by mutableStateOf("")
        private set

    var errorMessage by mutableStateOf<String?>(null)
        private set

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
        if (username.isBlank() || password.isBlank() || username.length > maxUnameLength || password.length > maxPasswdLength) {
            errorMessage = "Please enter a valid username and password"
            return
        }

        if (Native.verifyAccount(username, password)) {
            _navigation.value = NavigationEvent.ToMain
        }
    }

    fun onCreateAccountClicked() {
        _navigation.value = NavigationEvent.ToCreateAccount
    }
}