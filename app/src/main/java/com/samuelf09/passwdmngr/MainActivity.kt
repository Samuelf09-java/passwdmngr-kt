package com.samuelf09.passwdmngr

import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.tooling.preview.Preview
import com.samuelf09.passwdmngr.ui.screens.LoginScreen
import com.samuelf09.passwdmngr.ui.theme.PasswordManagerTheme

class MainActivity : ComponentActivity() {

    fun runLogin() {

    }

    fun runCreateAccount() {

    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.auto(
                lightScrim = Color.Transparent.toArgb(),
                darkScrim = Color.Transparent.toArgb()
            ),
            navigationBarStyle = SystemBarStyle.auto(
                lightScrim = Color.Transparent.toArgb(),
                darkScrim = Color.Transparent.toArgb()
            )
        )

        // disable autofill so password is not saved to regular autofill data
        window.decorView.importantForAutofill = View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS

        if (!Native.appInit(this.filesDir.absolutePath)) {
            Log.e("passwdmngr", "FATAL: App init failed!")
            Toast.makeText(this, "Initialization failed!", Toast.LENGTH_LONG).show()
            finishAndRemoveTask()
        }
        if (!Native.loadAccounts()) {
            Log.e("passwdmngr", "FATAL: Failed to load accounts!")
            Toast.makeText(this, "Failed to load accounts!", Toast.LENGTH_LONG).show()
            finishAndRemoveTask()
        }
        super.onCreate(savedInstanceState)
        setContent {
            PasswordManagerTheme {
                LoginScreen(onLogin = { runLogin() }, onCreateAccount = { runCreateAccount() })
            }
        }
    }
}