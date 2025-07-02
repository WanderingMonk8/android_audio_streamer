#include <jni.h>
#include <oboe/Oboe.h>
#include <memory>
#include <vector>
#include <mutex>

// Shared state between native and Java
struct AudioState {
    std::shared_ptr<oboe::AudioStream> stream;
    std::vector<float> audioBuffer;
    std::mutex bufferMutex;
    std::unique_ptr<AudioStreamCallback> callback;
    bool isInitialized = false;
};

class AudioStreamCallback : public oboe::AudioStreamCallback {
public:
    explicit AudioStreamCallback(AudioState* state) : mState(state) {}

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
        std::lock_guard<std::mutex> lock(mState->bufferMutex);
        float *inputBuffer = static_cast<float *>(audioData);
        
        // Store audio data in buffer
        int numSamples = numFrames * oboeStream->getChannelCount();
        mState->audioBuffer.assign(inputBuffer, inputBuffer + numSamples);
        return oboe::DataCallbackResult::Continue;
    }

private:
    AudioState* mState;
};

// Global audio state
AudioState gAudioState;

extern "C" {

JNIEXPORT void JNICALL
Java_com_example_audiocapture_OboeWrapper_nativeStartCapture(
    JNIEnv* env,
    jobject obj,
    jint sampleRate,
    jint channelCount
) {
    std::lock_guard<std::mutex> lock(gAudioState.bufferMutex);
    
    // Validate parameters
    if (sampleRate <= 0 || channelCount <= 0) {
        jclass exceptionClass = env->FindClass("java/lang/IllegalArgumentException");
        env->ThrowNew(exceptionClass, "Invalid sample rate or channel count");
        return;
    }
    
    // Stop existing stream if running
    if (gAudioState.stream) {
        gAudioState.stream->stop();
        gAudioState.stream->close();
        gAudioState.stream.reset();
    }
    
    // Create new callback
    gAudioState.callback = std::make_unique<AudioStreamCallback>(&gAudioState);
    
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(channelCount)
           ->setSampleRate(sampleRate)
           ->setCallback(gAudioState.callback.get());

    oboe::Result result = builder.openStream(gAudioState.stream);
    if (result != oboe::Result::OK) {
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        std::string errorMsg = "Failed to open Oboe stream: " + oboe::convertToText(result);
        env->ThrowNew(exceptionClass, errorMsg.c_str());
        return;
    }

    result = gAudioState.stream->requestStart();
    if (result != oboe::Result::OK) {
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        std::string errorMsg = "Failed to start Oboe stream: " + oboe::convertToText(result);
        env->ThrowNew(exceptionClass, errorMsg.c_str());
        gAudioState.stream->close();
        gAudioState.stream.reset();
        return;
    }
    
    gAudioState.isInitialized = true;
}

JNIEXPORT void JNICALL
Java_com_example_audiocapture_OboeWrapper_nativeStopCapture(
    JNIEnv* env,
    jobject obj
) {
    std::lock_guard<std::mutex> lock(gAudioState.bufferMutex);
    
    if (gAudioState.stream) {
        gAudioState.stream->stop();
        gAudioState.stream->close();
        gAudioState.stream.reset();
    }
    
    // Clear callback
    gAudioState.callback.reset();
    
    // Clear any remaining audio data
    gAudioState.audioBuffer.clear();
    gAudioState.isInitialized = false;
}

JNIEXPORT jobject JNICALL
Java_com_example_audiocapture_OboeWrapper_nativeGetAudioBuffer(
    JNIEnv* env,
    jobject obj
) {
    std::lock_guard<std::mutex> lock(gAudioState.bufferMutex);
    
    if (gAudioState.audioBuffer.empty()) {
        return nullptr;
    }

    // Create direct ByteBuffer from audio data
    jobject buffer = env->NewDirectByteBuffer(
        gAudioState.audioBuffer.data(),
        gAudioState.audioBuffer.size() * sizeof(float)
    );
    
    // Clear buffer after reading
    gAudioState.audioBuffer.clear();
    
    return buffer;
}