package com.samuelf09.passwdmngr

import android.annotation.SuppressLint
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.samuelf09.passwdmngr.ui.navigation.Routes
import com.samuelf09.passwdmngr.ui.screens.CreateAccountScreen
import com.samuelf09.passwdmngr.ui.screens.EditEntryScreen
import com.samuelf09.passwdmngr.ui.screens.LoginScreen
import com.samuelf09.passwdmngr.ui.screens.MainScreen
import com.samuelf09.passwdmngr.ui.theme.PasswordManagerTheme

class MainActivity : ComponentActivity() {

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
                AppNavHost()
            }
        }
    }

    @Composable
    fun AppNavHost() {
        val navController = rememberNavController()

        Scaffold { padding ->
            NavHost(
                navController = navController,
                startDestination = Routes.Login,
                enterTransition = { fadeIn() },
                exitTransition = { fadeOut() }
            ) {
                composable(Routes.Login) {
                    LoginScreen(
                        onLoginSuccess = { navController.navigate(Routes.MainApp) },
                        onCreateAccount = { navController.navigate(Routes.CreateAccount) }
                    )
                }

                composable(Routes.CreateAccount) {
                    CreateAccountScreen(
                        onAccountCreated = { navController.navigate(Routes.MainApp) },
                        onCancel = { navController.navigate(Routes.Login) }
                    )
                }

                composable(
                    route = Routes.MainAppFull,
                    arguments = listOf(navArgument("id") {
                        type = NavType.IntType
                        defaultValue = -1
                    } )
                ) {
                    backStackEntry -> val selectedId = backStackEntry.arguments?.getInt("id")
                    MainScreen(
                        onAddEntry = { navController.navigate(Routes.EditEntry) },
                        onLogout = { navController.navigate(Routes.Login) },
                        onEditEntry = { id -> navController.navigate("${Routes.EditEntry}?entryId=$id") },
                        selectedId = selectedId
                    )
                }

                composable (
                    route = Routes.EditEntryFull,
                    arguments = listOf(navArgument("entryId") {
                        type = NavType.IntType
                        defaultValue = -1
                    } )
                ) {
                    backStackEntry -> val entryId = backStackEntry.arguments?.getInt("entryId")
                    EditEntryScreen(
                        onSaveNewEntry = { navController.navigate(Routes.MainApp) },
                        onSaveEdits = { selectedId -> navController.navigate("${Routes.MainApp}?id=$selectedId") },
                        entry = if (entryId != null) Native.storageGetEntry(entryId) ?: emptyPasswdEntry() else emptyPasswdEntry()
                    )
                }
            }
        }
    }
}