#include "AndroidFramedSource.hh"
#include <GroupsockHelper.hh>

static int8_t buf[8192 * 100];
static int count = 0;

AndroidFramedSource *AndroidFramedSource::createNew(UsageEnvironment &env) {
    return new AndroidFramedSource(env);
}

EventTriggerId AndroidFramedSource::eventTriggerId = 0;

unsigned AndroidFramedSource::referenceCount = 0;

AndroidFramedSource::AndroidFramedSource(UsageEnvironment &env)
        : FramedSource(env) {
    if (referenceCount == 0) {
        // Any global initialization of the device would be done here:
        //%%% TO BE WRITTEN %%%
    }
    ++referenceCount;

    if (eventTriggerId == 0) {
        eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
    }
    onSubsessionOpen(true);
}

AndroidFramedSource::~AndroidFramedSource() {
    --referenceCount;
    if (referenceCount == 0) {
        envir().taskScheduler().deleteEventTrigger(eventTriggerId);
        eventTriggerId = 0;
    }
}

void AndroidFramedSource::doStopGettingFrames() {
    onSubsessionOpen(false);
}

void AndroidFramedSource::doGetNextFrame() {
    count = getNextFrame(buf);
    deliverFrame();
}

void AndroidFramedSource::deliverFrame0(void *clientData) {
    ((AndroidFramedSource *) clientData)->deliverFrame();
}

void AndroidFramedSource::deliverFrame() {
    if (!isCurrentlyAwaitingData()) return; // we're not ready for the data yet

    auto *newFrameDataStart = (u_int8_t *) buf; //%%% TO BE WRITTEN %%%
    auto newFrameSize = static_cast<unsigned int>(count); //%%% TO BE WRITTEN %%%

    // Deliver the data here:
    if (newFrameSize > fMaxSize) {
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = newFrameSize - fMaxSize;
    } else {
        fFrameSize = newFrameSize;
    }
    gettimeofday(&fPresentationTime,
                 nullptr); // If you have a more accurate time - e.g., from an encoder - then use that instead.
    // If the device is *not* a 'live source' (e.g., it comes instead from a file or buffer), then set "fDurationInMicroseconds" here.
    memmove(fTo, newFrameDataStart, fFrameSize);

    // After delivering the data, inform the reader that it is now available:
    FramedSource::afterGetting(this);
}