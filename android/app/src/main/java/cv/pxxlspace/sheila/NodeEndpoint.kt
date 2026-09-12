package cv.pxxlspace.sheila

sealed interface NodeEndpoint {
    data object Starting : NodeEndpoint
    data object Stopped : NodeEndpoint
    data object Blocked : NodeEndpoint

    data object Canonical : NodeEndpoint {
        const val url: String = "http://sheila.local"
    }

    data class IpFallback(
        val address: String,
        val reason: String
    ) : NodeEndpoint {
        val url: String get() = "http://$address"
    }
}

data class NodeUiState(
    val endpoint: NodeEndpoint = NodeEndpoint.Stopped,
    val vaultReady: Boolean = false,
    val nativeHealth: String = "not-started",
    val detail: String = "The Intelligence Node is stopped."
)
