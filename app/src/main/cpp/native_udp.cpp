#include <jni.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "NativeUDP"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static int udp_socket = -1;
static struct sockaddr_in server_addr;

extern "C" JNIEXPORT jint JNICALL
Java_com_example_audiocapture_network_NativeUdpSender_createSocket(JNIEnv *env, jobject thiz, jstring host, jint port) {
    const char *host_str = env->GetStringUTFChars(host, 0);
    
    LOGI("Creating native UDP socket for %s:%d", host_str, port);
    
    // Create UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        int error = errno;
        LOGE("Failed to create socket: %d, errno: %d (%s)", udp_socket, error, strerror(error));
        env->ReleaseStringUTFChars(host, host_str);
        return -1;
    }
    
    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host_str, &server_addr.sin_addr) <= 0) {
        LOGE("Invalid address: %s", host_str);
        close(udp_socket);
        udp_socket = -1;
        env->ReleaseStringUTFChars(host, host_str);
        return -1;
    }
    
    LOGI("Native UDP socket created successfully: %d", udp_socket);
    env->ReleaseStringUTFChars(host, host_str);
    return udp_socket;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_audiocapture_network_NativeUdpSender_sendPacket(JNIEnv *env, jobject thiz, jbyteArray data) {
    if (udp_socket < 0) {
        LOGE("Socket not created");
        return -1;
    }
    
    jsize len = env->GetArrayLength(data);
    jbyte *bytes = env->GetByteArrayElements(data, 0);
    
    LOGI("Sending %d bytes via native UDP", len);
    
    ssize_t sent = sendto(udp_socket, bytes, len, 0, 
                         (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    env->ReleaseByteArrayElements(data, bytes, 0);
    
    if (sent < 0) {
        LOGE("Failed to send packet: %zd", sent);
        return -1;
    }
    
    LOGI("Successfully sent %zd bytes", sent);
    return (jint)sent;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_audiocapture_network_NativeUdpSender_closeSocket(JNIEnv *env, jobject thiz) {
    if (udp_socket >= 0) {
        LOGI("Closing native UDP socket");
        close(udp_socket);
        udp_socket = -1;
    }
}