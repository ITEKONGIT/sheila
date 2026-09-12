package cv.pxxlspace.sheila

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat

class SheilaHostService : Service() {
    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> stopSelf()
            else -> startBaseline()
        }
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        NodeController.markStopped()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun startBaseline() {
        startForeground(NOTIFICATION_ID, notification("Baseline host integration is being checked"))
        NodeController.markStarting()

        val vaultResult = SecureVault.verify(this)
        if (vaultResult.isFailure) {
            NodeController.markBlocked(
                detail = "Secure private storage could not be opened.",
                nativeHealth = "not-checked"
            )
            return
        }

        val health = runCatching { NativeBridge.nativeHealth() }.getOrElse { "native-load-failed" }
        NodeController.markBlocked(
            detail = "Native host runtime and port-80 checks are the next integration gate.",
            nativeHealth = health,
            vaultReady = true
        )
        val manager = getSystemService(NotificationManager::class.java)
        manager.notify(NOTIFICATION_ID, notification("Secure vault verified; host runtime pending"))
    }

    private fun notification(detail: String): Notification =
        NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_lock_lock)
            .setContentTitle(getString(R.string.node_notification_title))
            .setContentText(detail)
            .setOngoing(true)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .build()

    private fun createNotificationChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID,
                getString(R.string.node_channel_name),
                NotificationManager.IMPORTANCE_LOW
            )
        )
    }

    companion object {
        const val ACTION_STOP = "cv.pxxlspace.sheila.action.STOP"
        private const val CHANNEL_ID = "sheila-node"
        private const val NOTIFICATION_ID = 1001
    }
}
