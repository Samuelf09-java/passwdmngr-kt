package com.samuelf09.passwdmngr.ui.screens

import android.util.Log
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.samuelf09.passwdmngr.PasswdEntry
import com.samuelf09.passwdmngr.viewmodel.MainViewModel

@Composable
fun SidebarItem(
    entry: PasswdEntry,
    selected: Boolean,
    onClick: () -> Unit) {
    val background = if (selected) {
        MaterialTheme.colorScheme.primary.copy(alpha = 0.15f)
    } else {
        Color.Transparent
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(background)
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 12.dp)
    ) {
        Text(
            text = entry.service ?: "",
            style = MaterialTheme.typography.bodyLarge,
            color = if (selected)
                MaterialTheme.colorScheme.primary
            else
                MaterialTheme.colorScheme.onSurface
        )
    }
}

@Composable
fun MainScreen(
    onAddEntry: () -> Unit,
    onEditEntry: (Int) -> Unit,
    onLogout: () -> Unit,
    selectedId: Int?,
    viewModel: MainViewModel = viewModel()
) {
    LaunchedEffect(selectedId) {
        viewModel.selectEntry(selectedId?.takeIf { it >= 0 })
    }
    val collapsed = viewModel.sidebarCollapsed
    val entries = viewModel.entries

    val sidebarWidth by animateDpAsState(
        targetValue = if (collapsed) 0.dp else LocalConfiguration.current.screenWidthDp.dp,
        label = "sidebarWidth"
    )

    val navEvent by viewModel.navigation.collectAsState()

    LaunchedEffect(navEvent) {
        when (navEvent) {
            MainViewModel.NavigationEvent.ToAddEntry -> {
                onAddEntry()
                viewModel.clearNavigation()
            }
            MainViewModel.NavigationEvent.ToLogin -> {
                onLogout()
                viewModel.clearNavigation()
            }
            null -> Unit
        }
    }

    Box(
        Modifier
            .fillMaxSize()
            .systemBarsPadding()
    ) {
        Row(Modifier.fillMaxWidth().align(Alignment.CenterEnd)) {

            Box(
                modifier = Modifier.weight(1f)
            ) {
                val selectedEntryId = viewModel.selectedEntryId
                if (selectedEntryId == null) {
                    NoEntrySelectedScreen()
                } else {
                    EntryScreen(
                        id = selectedEntryId,
                        onEdit = { id -> onEditEntry(id) },
                        onEntryDeleted = {
                            viewModel.reloadEntries()
                            viewModel.selectEntry(null)
                        }
                    )
                }
            }
        }

        IconButton(
            onClick = { viewModel.addEntry() },
            modifier = Modifier
                .align(Alignment.BottomStart)
                .padding(16.dp)
        ) {
            Icon(Icons.Default.Add, contentDescription = "New entry")
        }

        Box(
            modifier = Modifier
                .width(sidebarWidth)
                .fillMaxHeight()
                .background(MaterialTheme.colorScheme.surface)
        ) {
            if (!collapsed) {
                Column(Modifier.fillMaxSize()) {

                    Text(
                        "Select entry",
                        style = MaterialTheme.typography.titleLarge,
                        modifier = Modifier
                            .padding(16.dp)
                            .align(Alignment.CenterHorizontally)
                    )

                    Column(
                        modifier = Modifier
                            .weight(1f)
                            .verticalScroll(rememberScrollState())
                    ) {
                        val selectedEntryId = viewModel.selectedEntryId
                        entries.forEach { entry ->
                            SidebarItem(
                                entry = entry,
                                selected = entry.id == selectedEntryId,
                                onClick = {
                                    viewModel.selectEntry(entry.id)
                                    viewModel.toggleSidebar()
                                }
                            )
                        }
                    }

                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(Icons.Default.Add, contentDescription = null)
                        Spacer(Modifier.width(8.dp))
                        Text("New entry")
                    }
                }
            }
        }

        IconButton(
            onClick = { viewModel.toggleSidebar() },
            modifier = Modifier
                .align(Alignment.TopStart)
                .padding(16.dp)
        ) {
            Icon(Icons.Default.Menu, contentDescription = "Toggle sidebar")
        }
    }
}
