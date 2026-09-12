package cv.pxxlspace.sheila

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import java.nio.charset.StandardCharsets
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

data class SealedPayload(
    val iv: ByteArray,
    val ciphertext: ByteArray
)

object SecureVault {
    private const val provider = "AndroidKeyStore"
    private const val alias = "ssheila.intelligence-node.master"
    private const val transformation = "AES/GCM/NoPadding"

    private fun keyStore(): KeyStore = KeyStore.getInstance(provider).apply { load(null) }

    private fun getOrCreateKey(): SecretKey {
        val store = keyStore()
        val existing = store.getKey(alias, null) as? SecretKey
        if (existing != null) {
            return existing
        }

        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, provider).apply {
            init(
                KeyGenParameterSpec.Builder(
                    alias,
                    KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
                )
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256)
                    .build()
            )
        }.generateKey()
    }

    fun verify(context: Context): Result<Unit> = runCatching {
        val plaintext = "ssheila-vault-check".toByteArray(StandardCharsets.UTF_8)
        val cipher = Cipher.getInstance(transformation)
        cipher.init(Cipher.ENCRYPT_MODE, getOrCreateKey())
        val sealed = SealedPayload(cipher.iv, cipher.doFinal(plaintext))

        val decrypt = Cipher.getInstance(transformation)
        decrypt.init(
            Cipher.DECRYPT_MODE,
            getOrCreateKey(),
            GCMParameterSpec(128, sealed.iv)
        )
        check(decrypt.doFinal(sealed.ciphertext).contentEquals(plaintext)) {
            "Vault round-trip verification failed"
        }

        // Keep the context in the signature so the eventual vault can be
        // rooted in filesDir without changing the service boundary.
        check(context.filesDir.isDirectory) { "Private app storage is unavailable" }
    }

    fun seal(plaintext: ByteArray): SealedPayload {
        val cipher = Cipher.getInstance(transformation)
        cipher.init(Cipher.ENCRYPT_MODE, getOrCreateKey())
        return SealedPayload(cipher.iv, cipher.doFinal(plaintext))
    }

    fun open(payload: SealedPayload): ByteArray {
        val cipher = Cipher.getInstance(transformation)
        cipher.init(
            Cipher.DECRYPT_MODE,
            getOrCreateKey(),
            GCMParameterSpec(128, payload.iv)
        )
        return cipher.doFinal(payload.ciphertext)
    }
}
