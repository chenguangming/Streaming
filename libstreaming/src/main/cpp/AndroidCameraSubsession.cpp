#include <AndroidFramedSource.hh>
#include "AndroidCameraSubsession.hh"
#include "H264VideoRTPSink.hh"
#include "ByteStreamFileSource.hh"
#include "H264VideoStreamFramer.hh"


#define LOG_TAG "Rtsp-CameraSubsession"
#include "log.h"

AndroidCameraSubsession *AndroidCameraSubsession::createNew(UsageEnvironment &env) {
    return new AndroidCameraSubsession(env, False);
}

AndroidCameraSubsession::AndroidCameraSubsession(UsageEnvironment &env, Boolean reuseFirstSource)
        : OnDemandServerMediaSubsession(env, reuseFirstSource),
          fAuxSDPLine(nullptr), fDoneFlag(0), fDummyRTPSink(nullptr) {
}

AndroidCameraSubsession::~AndroidCameraSubsession() {
    delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void *clientData) {
    auto *subsess = (AndroidCameraSubsession *) clientData;
    subsess->afterPlayingDummy1();
}

void AndroidCameraSubsession::afterPlayingDummy1() {
    LOGE("afterPlayingDummy1");
    // Unschedule any pending 'checking' task:
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    // Signal the event loop that we're done:
    setDoneFlag();
}

static void checkForAuxSDPLine(void *clientData) {
    auto *subsess = (AndroidCameraSubsession *) clientData;
    subsess->checkForAuxSDPLine1();
}

void AndroidCameraSubsession::checkForAuxSDPLine1() {
    nextTask() = nullptr;

    char const *dasl;
    if (fAuxSDPLine != nullptr) {
        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (fDummyRTPSink != nullptr && (dasl = fDummyRTPSink->auxSDPLine()) != nullptr) {
        fAuxSDPLine = strDup(dasl);
        fDummyRTPSink = nullptr;

        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (!fDoneFlag) {
        // try again after a brief delay:
        int uSecsToDelay = 100000; // 100 ms
        nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                                 (TaskFunc *) checkForAuxSDPLine,
                                                                 this);
    }
}

char const *AndroidCameraSubsession::getAuxSDPLine(RTPSink *rtpSink, FramedSource *inputSource) {
    if (fAuxSDPLine != nullptr) return fAuxSDPLine; // it's already been set up (for a previous client)

    if (fDummyRTPSink == nullptr) { // we're not already setting it up for another, concurrent stream
        // Note: For H264 video files, the 'config' information ("profile-level-id" and "sprop-parameter-sets") isn't known
        // until we start reading the file.  This means that "rtpSink"s "auxSDPLine()" will be nullptr initially,
        // and we need to start reading data from our file until this changes.
        fDummyRTPSink = rtpSink;

        LOGV("startPlaying");
        // Start reading the file:
        fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

        // Check whether the sink's 'auxSDPLine()' is ready:
        checkForAuxSDPLine(this);
    }

    envir().taskScheduler().doEventLoop(&fDoneFlag);

    return fAuxSDPLine;
}

FramedSource *
AndroidCameraSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned &estBitrate) {
    estBitrate = 500; // kbps, estimate

    LOGV("createNewStreamSource");
    AndroidFramedSource* devSource = AndroidFramedSource::createNew(envir());

    return H264VideoStreamFramer::createNew(envir(), devSource);
}

RTPSink *AndroidCameraSubsession::createNewRTPSink(Groupsock *rtpGroupsock,
                                                   unsigned char rtpPayloadTypeIfDynamic,
                                                   FramedSource * /*inputSource*/) {
    LOGV("createNewRTPSink");
    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
