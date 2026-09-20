package com.samuelf09.passwdmngr.viewmodel

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import com.samuelf09.passwdmngr.Native
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class CreateAccountViewModel: ViewModel() {
    var username by mutableStateOf("")
    var password by mutableStateOf("")
    var confirmPassword by mutableStateOf("")

    var errorMessage by mutableStateOf<String?>(null)

    sealed class NavigationEvent {
        data object ToMain: NavigationEvent()
        data object ToLogin : NavigationEvent()
    }

    private val _navigation = MutableStateFlow<NavigationEvent?>(null)
    val navigation = _navigation.asStateFlow()
    fun clearNavigation() {
        _navigation.value = null
    }

    private fun isValidInput(input: String) = input.length in 4..32

    fun onUsernameChanged(newValue: String) {
        username = newValue
    }

    fun onPasswordChanged(newValue: String) {
        password = newValue
    }

    fun onConfirmPasswordChanged(newValue: String) {
        confirmPassword = newValue
    }

    fun onCreateAccountClicked() {
        if (!isValidInput(username) || !isValidInput(password) || !isValidInput(confirmPassword)) {
            errorMessage = "Please enter a valid username and password (4-32 characters)"
            return
        }

        if (password != confirmPassword) {
            errorMessage = "Passwords do not match!"
            return
        }

        if (!Native.createNewAccount(username, password)) {
            errorMessage = "Failed to create new account"
            // TODO: use numerical codes to determine cause of failure
            return
        }

        // TODO: setup as necessary
        _navigation.value = NavigationEvent.ToMain
    }

    fun onCancelClicked() {
        _navigation.value = NavigationEvent.ToLogin
    }
}