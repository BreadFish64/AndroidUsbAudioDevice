#include <jni.h>

#include <cstring>

static void I24toF32(float* dst, const int8_t* src, long long count) {
    constexpr float norm = (1 << 23);

    for (long long i = 0; i < count; ++i) {
        int i24_sample;
        std::memcpy(&i24_sample, src, sizeof(i24_sample));
        src += 3;
        // sign extend
        i24_sample <<= 8;
        i24_sample >>= 8;
        float f32_sample = static_cast<float>(i24_sample) / norm;
        dst[i] = f32_sample;
    }
}


extern "C" {
    JNIEXPORT void JNICALL Java_io_github_breadfish64_usbaudiosink_SampleConversionKt_I24toF32(
            JNIEnv *env, jclass clazz, jfloatArray dst, jlong dstOffset, jbyteArray src, jlong srcOffset, jlong count) {

        int8_t* native_src = env->GetByteArrayElements(src, nullptr);
        float* native_dst = env->GetFloatArrayElements(dst, nullptr);

        I24toF32(native_dst + dstOffset, native_src + srcOffset, count);

        env->ReleaseFloatArrayElements(dst, native_dst, 0);
        env->ReleaseByteArrayElements(src, native_src, JNI_ABORT);
    }
}

