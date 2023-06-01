#include <jni.h>
#include <string>

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include "AndroidFramedSource.hh"

#define LOG_TAG "RtspServer"
#include "log.h"

UsageEnvironment *uEnv;
H264VideoStreamFramer *videoSource;
RTPSink *videoSink;
FILE *file;
char const *inputFilename;

typedef struct context {
    JavaVM  *javaVM;
    jclass   RtspClz;
    jobject  RtspObj;
    jmethodID  getFrame;
} Context;
Context g_ctx;

int getFrame(int8_t* buf) {
    JavaVM *javaVM = g_ctx.javaVM;
    JNIEnv *env;

    jint res = javaVM->GetEnv((void **) &env, JNI_VERSION_1_6);
//    jclass clz = env->FindClass("com/example/rtsp/RtspServer");
//    jmethodID getFrame = env->GetStaticMethodID(clz, "getFrame", "()[B");
    jbyteArray arr = (jbyteArray) env->CallStaticObjectMethod(g_ctx.RtspClz, g_ctx.getFrame);
//    env->DeleteLocalRef(clz);
    int count = env->GetArrayLength(arr);
//    buf = env->GetByteArrayElements(arr, 0);
    env->GetByteArrayRegion(arr, 0, count, buf);

    env->DeleteLocalRef(arr);
    return count;
}

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.javaVM = vm;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }

    jclass clz = env->FindClass("com/photons/libstreaming/RtspServer");
    g_ctx.RtspClz = (jclass)env->NewGlobalRef(clz);
    g_ctx.getFrame = env->GetStaticMethodID(g_ctx.RtspClz, "getFrame", "()[B");

    return  JNI_VERSION_1_6;
}

extern "C" {
void play();
void afterPlaying(void * /*clientData*/);
}

extern "C" JNIEXPORT void JNICALL
Java_com_photons_libstreaming_RtspServer_loop(JNIEnv *env, jobject obj, jstring filename, jstring addr) {
    inputFilename = env->GetStringUTFChars(filename, NULL);
//    file = fopen(inputFilename, "rb");
//    if (!file) {
//        LOGE("couldn't open %s", inputFilename);
//        exit(1);
//    }
//    fclose(file);

    // Begin by setting up our usage environment:
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    uEnv = BasicUsageEnvironment::createNew(*scheduler);

    // Create 'groupsocks' for RTP and RTCP:
    struct sockaddr_storage destinationAddress;
    destinationAddress.ss_family = AF_INET;

    const char *_addr = env->GetStringUTFChars(addr, NULL);
    ((struct sockaddr_in&)destinationAddress).sin_addr.s_addr = ourIPv4Address(*uEnv);
    env->ReleaseStringUTFChars(addr, _addr);
    // Note: This is a multicast address.  If you wish instead to stream
    // using unicast, then you should use the "testOnDemandRTSPServer"
    // test program - not this test program - as a model.

    const unsigned short rtpPortNum = 18888;
    const unsigned short rtcpPortNum = rtpPortNum + 1;
    const unsigned char ttl = 255;

    const Port rtpPort(rtpPortNum);
    const Port rtcpPort(rtcpPortNum);

    Groupsock rtpGroupsock(*uEnv, destinationAddress, rtpPort, ttl);
//    rtpGroupsock.multicastSendOnly(); // we're a SSM source
    Groupsock rtcpGroupsock(*uEnv, destinationAddress, rtcpPort, ttl);
//    rtcpGroupsock.multicastSendOnly(); // we're a SSM source

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
                                                 NULL /* we're a server */);
    // Note: This starts RTCP running automatically

    RTSPServer *rtspServer = RTSPServer::createNew(*uEnv, 0);
    if (rtspServer == NULL) {
        LOGE("Failed to create RTSP server: %s", uEnv->getResultMsg());
        exit(1);
    }
    ServerMediaSession *sms = ServerMediaSession::createNew(*uEnv, "streamer",
                                                            inputFilename,
                                                            "Session streamed by \"testH264VideoStreamer\"");
    sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
    rtspServer->addServerMediaSession(sms);

    char *url = rtspServer->rtspURL(sms);
    LOGI("Play this stream using the URL \"%s\"", url);
    delete[] url;

    // Start the streaming:
    LOGI("Beginning streaming...");
    play();

    uEnv->taskScheduler().doEventLoop(); // does not return
}

void play() {
//    // Open the input file as a 'byte-stream file source':
//    ByteStreamFileSource *fileSource = ByteStreamFileSource::createNew(*uEnv,
//                                                                       inputFilename);
//    if (fileSource == NULL) {
//        LOGE("Unable to open file \"%s\" as a byte-stream file source", inputFilename);
//        exit(1);
//    }
//
//    FramedSource *videoES = fileSource;

    AndroidFramedSource* devSource
            = AndroidFramedSource::createNew(*uEnv);
    if (devSource == NULL)
    {

        LOGE("Unable to open source");
        exit(1);
    }

    FramedSource* videoES = devSource;

    // Create a framer for the Video Elementary Stream:
    videoSource = H264VideoStreamFramer::createNew(*uEnv, videoES);

    // Finally, start playing:
    LOGV("Beginning to read from file...");
    videoSink->startPlaying(*videoSource, afterPlaying, videoSink);
}

void afterPlaying(void * /*clientData*/) {
    LOGV("...done reading from file");
    videoSink->stopPlaying();
    Medium::close(videoSource);

    //play();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_photons_libstreaming_RtspServer_unicast(JNIEnv *env, jobject instance, jstring fileName_) {
    const char *inputFilename = env->GetStringUTFChars(fileName_, 0);
    FILE *file = fopen(inputFilename, "rb");
    if (!file) {
        LOGE("couldn't open %s", inputFilename);
        exit(1);
    }
    fclose(file);

    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env_ = BasicUsageEnvironment::createNew(*scheduler);
    UserAuthenticationDatabase* authDB = NULL;
    // Create the RTSP server:
    RTSPServer* rtspServer = RTSPServer::createNew(*env_, 8554, authDB);
    if (rtspServer == NULL) {
        LOGE("Failed to create RTSP server: %s", env_->getResultMsg());
        exit(1);
    }
    char const* descriptionString = "Session streamed by \"testOnDemandRTSPServer\"";
    char const* streamName = "v";
    ServerMediaSession* sms  = ServerMediaSession::createNew(*env_, streamName, streamName, descriptionString);
    sms->addSubsession(H264VideoFileServerMediaSubsession::createNew(*env_, inputFilename, True));
    rtspServer->addServerMediaSession(sms);

    char* url = rtspServer->rtspURL(sms);
    LOGE("%s stream, from the file %s ",streamName, inputFilename);
    LOGE("Play this stream using the URL: %s", url);
    delete[] url;

    env->ReleaseStringUTFChars(fileName_, inputFilename);
    env_->taskScheduler().doEventLoop(); // does not return
}

int AndroidFramedSource::getNextFrame(int8_t* buf) {
    int c = getFrame(buf);
//    LOGE( "getNextFrame %d ",c);
    return c;
}