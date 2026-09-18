package com.samuelf09.passwdmngr.ui.screens

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.autofill.ContentDataType
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import com.samuelf09.passwdmngr.R

@Composable
fun LoginScreen(
    onLogin: () -> Unit,
    onCreateAccount: () -> Unit
) {
    val focusManager = LocalFocusManager.current;

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
            .padding(16.dp)
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null
            ) {
                focusManager.clearFocus()
            },
        contentAlignment = Alignment.Center
    ) {

        Card(
            modifier = Modifier
                .widthIn(max = 360.dp)
                .padding(16.dp),
            shape = RoundedCornerShape(16.dp),
            elevation = CardDefaults.cardElevation(defaultElevation = 7.dp),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surface
            )
        ) {
            Column(
                modifier = Modifier
                    .padding(24.dp)
                    .fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {

                Image(
                    painter = painterResource(R.drawable.logo),
                    contentDescription = "App Logo",
                    modifier = Modifier.size(128.dp)
                )

                var username by remember { mutableStateOf("") }
                OutlinedTextField(
                    value = username,
                    onValueChange = { username = it },
                    placeholder = { Text("Enter username…") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )

                var password by remember { mutableStateOf("") }
                var showPassword by remember { mutableStateOf(false) }

                OutlinedTextField(
                    value = password,
                    onValueChange = { password = it },
                    placeholder = { Text("Enter password…") },
                    singleLine = true,
                    visualTransformation =
                        if (showPassword) VisualTransformation.None
                        else PasswordVisualTransformation(),
                    modifier = Modifier
                        .fillMaxWidth(),
                    keyboardOptions = KeyboardOptions(
                        autoCorrectEnabled = false,
                        keyboardType = KeyboardType.Password
                    ),
                    keyboardActions = KeyboardActions.Default,
                    trailingIcon = {
                        IconButton(onClick = { showPassword = !showPassword }) {
                            Icon(
                                imageVector = if (showPassword)
                                    Icons.Default.Visibility
                                else Icons.Default.VisibilityOff,
                                contentDescription = null
                            )
                        }
                    }
                )

                Button(
                    onClick = onLogin,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Login")
                }

                TextButton(
                    onClick = onCreateAccount,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Create new account")
                }
            }
        }
    }
}