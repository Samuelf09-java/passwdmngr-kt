package com.samuelf09.passwdmngr.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.samuelf09.passwdmngr.PasswdEntry
import com.samuelf09.passwdmngr.emptyPasswdEntry
import com.samuelf09.passwdmngr.viewmodel.EditEntryViewModel

@Composable
fun EditEntryScreen(
    onSaveNewEntry: () -> Unit,
    onSaveEdits: (Int) -> Unit,
    entry: PasswdEntry = emptyPasswdEntry(),
    viewModel: EditEntryViewModel = viewModel()
) {
    val focusManager = LocalFocusManager.current

    LaunchedEffect(entry.id) {
        viewModel.init(entry)
    }

    val service = viewModel.service
    val username = viewModel.username
    val password = viewModel.password
    val notes = viewModel.notes

    val errorMessage = viewModel.errorMessage

    val navEvent by viewModel.navigation.collectAsState()

    LaunchedEffect(navEvent) {
        when (navEvent) {
            EditEntryViewModel.NavigationEvent.ToMain -> {
                onSaveNewEntry()
                viewModel.clearNavigation()
            }
            EditEntryViewModel.NavigationEvent.ToMainWithId -> {
                onSaveEdits(viewModel.id)
                viewModel.clearNavigation()
            }
            null -> Unit
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
            .padding(8.dp)
            .systemBarsPadding()
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null
            ) {
                focusManager.clearFocus()
            },
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .padding(24.dp)
                .fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text("Service")
            OutlinedTextField(
                value = service,
                onValueChange = viewModel::onServiceChanged,
                singleLine = true,
                modifier = Modifier
                    .fillMaxWidth(),
            )

            Text("Username")
            OutlinedTextField(
                value = username,
                onValueChange = viewModel::onUsernameChanged,
                singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
            )

            Text("Password")
            OutlinedTextField(
                value = password,
                onValueChange = viewModel::onPasswordChanged,
                singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
            )

            Text("Notes")
            OutlinedTextField(
                value = notes,
                onValueChange = viewModel::onNotesChanged,
                singleLine = false,
                modifier = Modifier.fillMaxWidth(),
            )

            if (errorMessage != null) {
                Text(
                    text = errorMessage,
                    color = MaterialTheme.colorScheme.error,
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.padding(top = 8.dp)
                )
            }
        }

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.BottomCenter)
        ) {
            Button(
                onClick = viewModel::onSaveClicked,
                modifier = Modifier
                    .width((LocalConfiguration.current.screenWidthDp.dp - 20.dp) / 2)
            ) {
                Text("Save")
            }

            Spacer(modifier = Modifier.width(16.dp))

            TextButton(
                onClick = viewModel::onCancelClicked,
                modifier = Modifier
                    .width((LocalConfiguration.current.screenWidthDp.dp - 20.dp) / 2)
            ) {
                Text("Cancel")
            }
        }
    }
}