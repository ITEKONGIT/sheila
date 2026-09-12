package cv.pxxlspace.sheila

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

object NodeController {
    private val mutableState = MutableStateFlow(NodeUiState())
    val state: StateFlow<NodeUiState> = mutableState.asStateFlow()

    fun markStarting() {
        mutableState.value = NodeUiState(
            endpoint = NodeEndpoint.Starting,
            detail = "Checking secure storage and host compatibility…"
        )
    }

    fun markBlocked(detail: String, nativeHealth: String = "unknown", vaultReady: Boolean = false) {
        mutableState.value = NodeUiState(
            endpoint = NodeEndpoint.Blocked,
            vaultReady = vaultReady,
            nativeHealth = nativeHealth,
            detail = detail
        )
    }

    fun markStopped() {
        mutableState.value = NodeUiState(
            endpoint = NodeEndpoint.Stopped,
            detail = "The Intelligence Node is stopped."
        )
    }
}
