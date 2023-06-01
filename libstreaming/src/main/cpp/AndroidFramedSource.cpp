/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2018 Live Networks, Inc.  All rights reserved.
// A template for a MediaSource encapsulating an audio/video input device
//
// NOTE: Sections of this code labeled "%%% TO BE WRITTEN %%%" are incomplete, and need to be written by the programmer
// (depending on the features of the particular device).
// Implementation

#include "AndroidFramedSource.hh"
#include <GroupsockHelper.hh> // for "gettimeofday()"
static int8_t buf[8192*4];
static int count = 0;
AndroidFramedSource*
AndroidFramedSource::createNew(UsageEnvironment& env) {
  return new AndroidFramedSource(env);
}

EventTriggerId AndroidFramedSource::eventTriggerId = 0;

unsigned AndroidFramedSource::referenceCount = 0;

AndroidFramedSource::AndroidFramedSource(UsageEnvironment& env)
  : FramedSource(env) {
  if (referenceCount == 0) {
    // Any global initialization of the device would be done here:
    //%%% TO BE WRITTEN %%%
  }
  ++referenceCount;

  if (eventTriggerId == 0) {
    eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
  }
}

AndroidFramedSource::~AndroidFramedSource() {
  --referenceCount;
  if (referenceCount == 0) {
    envir().taskScheduler().deleteEventTrigger(eventTriggerId);
    eventTriggerId = 0;
  }
}

void AndroidFramedSource::doGetNextFrame() {
  count = getNextFrame(buf);
  deliverFrame();
}

void AndroidFramedSource::deliverFrame0(void* clientData) {
  ((AndroidFramedSource*)clientData)->deliverFrame();
}

void AndroidFramedSource::deliverFrame() {
  if (!isCurrentlyAwaitingData()) return; // we're not ready for the data yet

  u_int8_t* newFrameDataStart = (u_int8_t*)buf; //%%% TO BE WRITTEN %%%
  unsigned newFrameSize = static_cast<unsigned int>(count); //%%% TO BE WRITTEN %%%

  // Deliver the data here:
  if (newFrameSize > fMaxSize) {
    fFrameSize = fMaxSize;
    fNumTruncatedBytes = newFrameSize - fMaxSize;
  } else {
    fFrameSize = newFrameSize;
  }
  gettimeofday(&fPresentationTime, NULL); // If you have a more accurate time - e.g., from an encoder - then use that instead.
  // If the device is *not* a 'live source' (e.g., it comes instead from a file or buffer), then set "fDurationInMicroseconds" here.
  memmove(fTo, newFrameDataStart, fFrameSize);

  // After delivering the data, inform the reader that it is now available:
  FramedSource::afterGetting(this);
}