#include <jni.h>

namespace {

jstring make_string(JNIEnv* env, const char* value) {
    return env->NewStringUTF(value);
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_cv_pxxlspace_sheila_NativeBridge_nativeHealth(JNIEnv* env, jobject) {
    return make_string(env, "android-jni-baseline");
}
