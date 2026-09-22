package com.samuelf09.passwdmngr.ui.screens

import android.util.Log
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DeleteForever
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.lifecycle.viewmodel.compose.viewModel
import com.samuelf09.passwdmngr.viewmodel.EntryViewModel

@Composable
fun EntryScreen(
    id: Int,
    onEdit: (Int) -> Unit,
    onEntryDeleted: () -> Unit,
    viewModel: EntryViewModel = viewModel(key = "entry-$id"),
) {
    LaunchedEffect(id) {
        viewModel.init(id)
    }

    val showDeleteDialog = viewModel.showDeleteDialog
    val errorMessage = viewModel.errorMessage
    val navEvent by viewModel.navigation.collectAsState()

    LaunchedEffect(navEvent) {
        when (navEvent) {
            EntryViewModel.NavigationEvent.ToEdit -> {
                onEdit(viewModel.entry!!.id)
                viewModel.clearNavigation()
            }
            EntryViewModel.NavigationEvent.ToMain -> {
                onEntryDeleted()
                viewModel.clearNavigation()
            }
            null -> Unit
        }
    }

    if (showDeleteDialog) {
        Dialog(
            onDismissRequest = { viewModel.showDeleteDialog = false }
        ) {
            Card {
                Column(
                    modifier = Modifier.padding(20.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text("Delete entry", style = MaterialTheme.typography.titleLarge)
                    Spacer(Modifier.padding(10.dp))
                    Text("Are you sure you want to delete '${viewModel.entry!!.service}'?\nThis action is permanent and cannot be undone")

                    Row(
                        horizontalArrangement = Arrangement.spacedBy(16.dp)
                    ) {
                        TextButton(
                            onClick = { viewModel.showDeleteDialog = false }
                        ) {
                            Text("Cancel")
                        }
                        TextButton(
                            onClick = {
                                viewModel.showDeleteDialog = false
                                viewModel.deleteEntry()
                            }
                        ) {
                            Text("Confirm")
                        }
                    }
                }
            }
        }
    }

    if (viewModel.entry == null) {
        Column(
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(
                text = "Loading…",
                modifier = Modifier.align(Alignment.CenterHorizontally)
            )
        }
        return
    }

    Box (
        modifier = Modifier
            .fillMaxSize()
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
        ) {
            Text(
                text = "Service: ${viewModel.entry!!.service}",
                modifier = Modifier.align(Alignment.CenterHorizontally)
            )
            Text(
                text = "Username: ${viewModel.entry!!.username}",
                modifier = Modifier.align(Alignment.CenterHorizontally)
            )
            Text(
                text = "Password: ${viewModel.entry!!.password}",
                modifier = Modifier.align(Alignment.CenterHorizontally)
            )
            if (!viewModel.entry!!.notes.isNullOrBlank()) {
                Text(
                    text = "Notes: ${viewModel.entry!!.notes}",
                    modifier = Modifier.align(Alignment.CenterHorizontally)
                )
            }
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
            modifier = Modifier.align(Alignment.BottomEnd)
        ) {
            IconButton(
                onClick = viewModel::onEditClicked
            ) {
                Icon(Icons.Default.Edit, contentDescription = "Edit entry")
            }

            IconButton(
                onClick = viewModel::onDeleteClicked
            ) {
                Icon(Icons.Default.DeleteForever, contentDescription = "Delete entry (permanent)")
            }
        }
    }
}