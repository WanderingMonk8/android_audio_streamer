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
Java_com_example_audiocapture_OboeWrapper_startCapture(
    JNIEnv* env,
    jobject obj,
    jint sampleRate,
    jint channelCount
) {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(channelCount)
           ->setSampleRate(sampleRate)
           ->setCallback(new AudioStreamCallback(&gAudioState));

    oboe::Result result = builder.openStream(gAudioState.stream);
    if (result != oboe::Result::OK) {
        // TODO: Handle error
        return;
    }

    gAudioState.stream->requestStart();
}

JNIEXPORT jobject JNICALL
Java_com_example_audiocapture_OboeWrapper_getAudioBuffer(
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