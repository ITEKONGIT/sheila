package cv.pxxlspace.sheila

object NativeBridge {
    init {
        System.loadLibrary("sheila_android")
    }

    external fun nativeHealth(): String
}
