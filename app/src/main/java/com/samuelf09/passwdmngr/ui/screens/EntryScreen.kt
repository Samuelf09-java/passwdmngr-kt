package com.samuelf09.passwdmngr.ui.screens

import androidx.compose.foundation.layout.Box
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.lifecycle.viewmodel.compose.viewModel
import com.samuelf09.passwdmngr.viewmodel.EntryViewModel

@Composable
fun EntryScreen(
    viewModel: EntryViewModel = viewModel(),
    id: Int
) {
    Box {
        Text("Service: ")
        Text("Username: ")
        Text("Password: ")
        Text("Notes: ")
    }
}