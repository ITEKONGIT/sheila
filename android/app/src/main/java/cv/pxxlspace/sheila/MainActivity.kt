package cv.pxxlspace.sheila

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.collectAsState
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat

class MainActivity : ComponentActivity() {
    private val notificationPermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SheilaTheme {
                val state by NodeController.state.collectAsState()
                IntelligenceNodeScreen(
                    state = state,
                    onStart = ::startNode,
                    onStop = ::stopNode
                )
            }
        }
    }

    private fun startNode() {
        if (Build.VERSION.SDK_INT >= 33 &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) !=
            PackageManager.PERMISSION_GRANTED
        ) {
            notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
        }

        NodeController.markStarting()
        ContextCompat.startForegroundService(
            this,
            Intent(this, SheilaHostService::class.java)
        )
    }

    private fun stopNode() {
        stopService(Intent(this, SheilaHostService::class.java))
    }
}

@Composable
private fun SheilaTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = androidx.compose.material3.darkColorScheme(
            primary = Color(0xFFD6FF55),
            onPrimary = Color(0xFF15170F),
            background = Color(0xFF0E100D),
            surface = Color(0xFF151713)
        ),
        content = content
    )
}

@Composable
fun IntelligenceNodeScreen(
    state: NodeUiState,
    onStart: () -> Unit,
    onStop: () -> Unit
) {
    Surface(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(24.dp),
            verticalArrangement = Arrangement.Center
        ) {
            Text("SHEILA", style = MaterialTheme.typography.labelLarge)
            Text("Intelligence Node", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.height(20.dp))

            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(18.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.padding(20.dp)) {
                    Text(endpointLabel(state.endpoint), style = MaterialTheme.typography.titleLarge)
                    Spacer(Modifier.height(8.dp))
                    Text(state.detail, style = MaterialTheme.typography.bodyMedium)
                    Spacer(Modifier.height(12.dp))
                    Text("Vault: ${if (state.vaultReady) "verified" else "not verified"}")
                    Text("Native bridge: ${state.nativeHealth}")
                }
            }

            Spacer(Modifier.height(18.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = onStart) { Text("Start node") }
                OutlinedButton(onClick = onStop) { Text("Stop") }
            }
        }
    }
}

private fun endpointLabel(endpoint: NodeEndpoint): String = when (endpoint) {
    NodeEndpoint.Starting -> "Checking node…"
    NodeEndpoint.Stopped -> "Node stopped"
    NodeEndpoint.Blocked -> "Node blocked"
    NodeEndpoint.Canonical -> NodeEndpoint.Canonical.url
    is NodeEndpoint.IpFallback -> endpoint.url
}

@Preview(showBackground = true, widthDp = 360, heightDp = 760)
@Composable
private fun IntelligenceNodePreview() {
    SheilaTheme {
        IntelligenceNodeScreen(
            state = NodeUiState(
                endpoint = NodeEndpoint.Canonical,
                vaultReady = true,
                nativeHealth = "android-jni-baseline",
                detail = "The Intelligence Node is ready on the local network."
            ),
            onStart = {},
            onStop = {}
        )
    }
}
