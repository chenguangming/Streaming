#include <jni.h>
#include <string>

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <AndroidCameraSubsession.hh>

#include "AndroidFramedSource.hh"

#define LOG_TAG "Rtsp-Server"
#include "log.h"

// only for multicast

H264VideoStreamFramer *videoSource;
RTPSink *videoSink;

typedef struct context {
    JavaVM *javaVM;
    jobject RtspObj;
    jmethodID getFrame;
    jmethodID onSubsessionStateChanged;
} Context;
Context gServerContext;

int getFrame(int8_t* buf) {
    JavaVM *javaVM = gServerContext.javaVM;
    JNIEnv *env;

    javaVM->GetEnv((void **) &env, JNI_VERSION_1_6);
    jbyteArray arr = (jbyteArray) env->CallObjectMethod(gServerContext.RtspObj, gServerContext.getFrame);
    int count = env->GetArrayLength(arr);
    env->GetByteArrayRegion(arr, 0, count, buf);
    env->DeleteLocalRef(arr);
    LOGV("getFrame %d", count);

    return count;
}

void onSubsessionStateChanged(bool isOpen) {
    JavaVM *javaVM = gServerContext.javaVM;
    JNIEnv *env;

    javaVM->GetEnv((void **) &env, JNI_VERSION_1_6);
    env->CallVoidMethod(gServerContext.RtspObj, gServerContext.onSubsessionStateChanged, isOpen);
    LOGV("onSubsessionStateChanged %d", isOpen);
}

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    memset(&gServerContext, 0, sizeof(gServerContext));

    gServerContext.javaVM = vm;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }

    jclass clz = env->FindClass("com/photons/libstreaming/RtspServer");
    gServerContext.getFrame = env->GetMethodID(clz, "getFrame", "()[B");
    gServerContext.onSubsessionStateChanged = env->GetMethodID(clz, "onSubsessionStateChanged", "(Z)V");

    return JNI_VERSION_1_6;
}

extern "C" {
void play(UsageEnvironment *uEnv);
void afterPlaying(void * /*clientData*/);
}

extern "C" JNIEXPORT void JNICALL
Java_com_photons_libstreaming_RtspServer_multicast(JNIEnv *env, jobject obj) {
    gServerContext.RtspObj = env->NewGlobalRef(obj);

    // Begin by setting up our usage environment:
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *uEnv = BasicUsageEnvironment::createNew(*scheduler);

    // Create 'groupsocks' for RTP and RTCP:
    struct sockaddr_storage destinationAddress;
    destinationAddress.ss_family = AF_INET;

    ((struct sockaddr_in&)destinationAddress).sin_addr.s_addr = chooseRandomIPv4SSMAddress(*uEnv);
    // Note: This is a multicast address.  If you wish instead to stream
    // using unicast, then you should use the "testOnDemandRTSPServer"
    // test program - not this test program - as a model.

    const unsigned short rtpPortNum = 18888;
    const unsigned short rtcpPortNum = rtpPortNum + 1;
    const unsigned char ttl = 255;

    const Port rtpPort(rtpPortNum);
    const Port rtcpPort(rtcpPortNum);

    Groupsock rtpGroupsock(*uEnv, destinationAddress, rtpPort, ttl);
    rtpGroupsock.multicastSendOnly(); // we're a SSM source
    Groupsock rtcpGroupsock(*uEnv, destinationAddress, rtcpPort, ttl);
    rtcpGroupsock.multicastSendOnly(); // we're a SSM source

    // Create a 'H264 Video RTP' sink from the RTP 'groupsock':
    OutPacketBuffer::maxSize = 100000;
    videoSink = H264VideoRTPSink::createNew(*uEnv, &rtpGroupsock, 96);

    // Create (and start) a 'RTCP instance' for this RTP sink:
    const unsigned estimatedSessionBandwidth = 500; // in kbps; for RTCP b/w share
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen + 1];
    gethostname((char *) CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0'; // just in case
    RTCPInstance *rtcp = RTCPInstance::createNew(*uEnv, &rtcpGroupsock,
                                                 estimatedSessionBandwidth, CNAME, videoSink,
                                                 NULL /* we're a server */,
                                                 True /* we're a SSM source */);
    // Note: This starts RTCP running automatically

    RTSPServer *rtspServer = RTSPServer::createNew(*uEnv, 8554);
    if (rtspServer == nullptr) {
        LOGE("Failed to create RTSP server: %s", uEnv->getResultMsg());
        exit(1);
    }
    ServerMediaSession *sms = ServerMediaSession::createNew(*uEnv, "streamer",
                                                            "camera",
                                                            "Session streamed by \"testH264VideoStreamer\"");
    sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
    rtspServer->addServerMediaSession(sms);

    char *url = rtspServer->rtspURL(sms);
    LOGI("Play this stream using the URL \"%s\"", url);
    delete[] url;

    // Start the streaming:
    LOGI("Beginning streaming...");
    play(uEnv);

    uEnv->taskScheduler().doEventLoop(); // does not return
}

void play(UsageEnvironment *uEnv) {
    AndroidFramedSource* devSource = AndroidFramedSource::createNew(*uEnv);
    if (devSource == nullptr) {
        LOGE("Unable to open source");
        return;
    }

    FramedSource* videoES = devSource;

    // Create a framer for the Video Elementary Stream:
    videoSource = H264VideoStreamFramer::createNew(*uEnv, videoES);

    // Finally, start playing:
    LOGV("startPlaying...");
    videoSink->startPlaying(*videoSource, afterPlaying, videoSink);
}

void afterPlaying(void * /*clientData*/) {
    LOGV("afterPlaying");
    videoSink->stopPlaying();
    Medium::close(videoSource);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_photons_libstreaming_RtspServer_unicast(JNIEnv *env, jobject obj) {
    gServerContext.RtspObj = env->NewGlobalRef(obj);

    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env_ = BasicUsageEnvironment::createNew(*scheduler);
    // Create the RTSP server:
    RTSPServer* rtspServer = RTSPServer::createNew(*env_, 8554, nullptr);
    if (rtspServer == nullptr) {
        LOGE("Failed to create RTSP server: %s", env_->getResultMsg());
        return;
    }
    char const* descriptionString = "Session streamed by \"Android CameraX\"";
    char const* streamName = "streamer";
    ServerMediaSession* sms = ServerMediaSession::createNew(*env_, streamName, streamName, descriptionString);

#if 1
    sms->addSubsession(AndroidCameraSubsession::createNew(*env_));
    rtspServer->addServerMediaSession(sms);
#else
    sms->addSubsession(H264VideoFileServerMediaSubsession
                       ::createNew(*env_, "/data/local/tmp/test.264", True));
    rtspServer->addServerMediaSession(sms);
#endif

    char* url = rtspServer->rtspURL(sms);
    LOGE("Play this stream using the URL: %s", url);
    delete[] url;

    env_->taskScheduler().doEventLoop(); // does not return
}

int AndroidFramedSource::getNextFrame(int8_t* buf) {
    return getFrame(buf);
}

void AndroidFramedSource::onSubsessionOpen(bool open) {
    onSubsessionStateChanged(open);
}