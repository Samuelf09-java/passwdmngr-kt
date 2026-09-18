package com.samuelf09.passwdmngr.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.samuelf09.passwdmngr.PasswdEntry
import com.samuelf09.passwdmngr.viewmodel.MainViewModel

@Composable
fun Sidebar(
    entries: List<PasswdEntry>,
    collapsed: Boolean,
    selectedEntryId: Int?,
    onToggleSidebar: () -> Unit,
    onEntrySelected: (Int) -> Unit
) {
    Column(
        modifier = Modifier
            .width(if (collapsed) 56.dp else 240.dp)
            .fillMaxHeight()
            .background(MaterialTheme.colorScheme.surface)
    ) {
        IconButton(onClick = onToggleSidebar) {
            Icon(Icons.Default.Menu, contentDescription = "Toggle sidebar")
        }

        if (!collapsed) {
            entries.forEach { entry ->
//                SidebarItem(
//                    entry = entry,
//                    selected = entry.id == selectedEntryId,
//                    onClick = { onEntrySelected(entry.id) }
//                )
            }
        }
    }
}

@Composable
fun MainAppScreen(
    viewModel: MainViewModel = viewModel()
) {
    val selectedEntryId = viewModel.selectedEntryId

    Row(Modifier.fillMaxSize()) {

        Sidebar(
            entries = viewModel.entries,
            selectedEntryId = selectedEntryId,
            onEntrySelected = viewModel::selectEntry,
            collapsed = viewModel.sidebarCollapsed,
            onToggleSidebar = viewModel::toggleSidebar
        )

        Box(Modifier.weight(1f)) {
            if (selectedEntryId == null) {
                NoEntrySelectedScreen()
            } else {
                EntryScreen(id = selectedEntryId)
            }
        }
    }
}
