    //
// Created by ezra on 4/21/18.
//

#include <cassert>
#include <string.h>
#include <limits>

#include "softcut/Interpolate.h"
#include "softcut/FadeCurves.h"
#include "softcut/SubHead.h"
#include "softcut/Utilities.h"

using namespace softcut;

void SubHead::init(FadeCurves *fc) {
    fadeCurves = fc;
    phase_ = 0;
    fade_ = 0;
    trig_ = 0;
    state_ = Stopped;
    resamp_.setPhase(0);
    inc_dir_ = 1;
    recOffset_ = -8;
}

Action SubHead::updatePhase(phase_t start, phase_t end, bool loop) {
    Action res = None;
    trig_ = 0.f;
    phase_t p;
    switch(state_) {
        case FadeIn:
        case FadeOut:
        case Playing:
            p = phase_ + rate_;
            if(active_) {
                // FIXME: should refactor this a bit.
                if (rate_ > 0.f) {
                    if (p > end || p < start) {
                        if (loop) {
                            trig_ = 1.f;
                            res = LoopPos;
                        } else {
                            state_ = FadeOut;
                            res = Stop;
                        }
                    }
                } else { // negative rate
                    if (p > end || p < start) {
                        if(loop) {
                            trig_ = 1.f;
                            res = LoopNeg;
                        } else {
                            state_ = FadeOut;
                            res = Stop;
                        }
                    }
                } // rate sign check
            } // /active check
            phase_ = p;
            break;
        case Stopped:
        default:
            ;; // nothing to do
    }
    return res;
}

void SubHead::updateFade(float inc) {
    switch(state_) {
        case FadeIn:
            fade_ += inc;
            if (fade_ > 1.f) {
                fade_ = 1.f;
                state_ = Playing;
            }
            break;
        case FadeOut:
            fade_ -= inc;
            if (fade_ < 0.f) {
                fade_ = 0.f;
                state_ = Stopped;
            }
            break;
        case Playing:
        case Stopped:
        default:;; // nothing to do
    }
}

#if 0
/// test: no resampling
void Subhead::poke(float in, float pre, float rec, int numFades) {
    sample_t* p = &buf_[static_cast<unsigned int>(phase_)&bufMask_];
    *p *= pre;
    *p += (in * rec);
}
#else
void SubHead::poke(float in, float pre, float rec) {
    // FIXME: since there's never really a reason to not push input, or to reset input ringbuf,
    // it follows that all resamplers could share an input ringbuf
    int nframes = resamp_.processFrame(in);

    if(state_ == Stopped) {
        return;
    }

    // assert(fade_ >= 0.f && fade_ <= 1.f /* bad fade coefficient in poke() */);

    preFade_ = pre + (1.f-pre) * fadeCurves->getPreFadeValue(fade_);
    recFade_ = rec * fadeCurves->getRecFadeValue(fade_);

    sample_t y; // write value
    const sample_t* src = resamp_.output();

    for(int i=0; i<nframes; ++i) {
        y = src[i];

#if 1 // soft clipper
        y = clip_.processSample(y);
#endif
#if 0 // lowpass filter
        lpf_.processSample(&y);
#endif
        sample_t &dst = buf_[static_cast<size_t>(wrIdx_) * stride_];
        dst *= preFade_;
        dst += y * recFade_;

        wrIdx_ = wrapBufIndex(wrIdx_ + inc_dir_);
    }
}
#endif

float SubHead::peek() {
    return peek4();
}

float SubHead::peek4() {
    int phase1 = static_cast<int>(phase_);
    int phase0 = phase1 - 1;
    int phase2 = phase1 + 1;
    int phase3 = phase1 + 2;

    float y0 = buf_[static_cast<size_t>(wrapBufIndex(phase0)) * stride_];
    float y1 = buf_[static_cast<size_t>(wrapBufIndex(phase1)) * stride_];
    float y3 = buf_[static_cast<size_t>(wrapBufIndex(phase3)) * stride_];
    float y2 = buf_[static_cast<size_t>(wrapBufIndex(phase2)) * stride_];

    auto x = static_cast<float>(phase_ - (float)phase1);
    return Interpolate::hermite<float>(x, y0, y1, y2, y3);
}

// softkut patch: wrap buffers of any length, not only powers of two. Indices
// are almost always in range, so the modulo runs only off the fast path.
unsigned int SubHead::wrapBufIndex(int x) {
    if (static_cast<unsigned int>(x) < bufFrames_) return static_cast<unsigned int>(x);
    if (bufFrames_ == 0) return 0;      // no buffer yet (a cut before setBuffer)
    const int n = static_cast<int>(bufFrames_);
    const int m = x % n;
    return static_cast<unsigned int>(m < 0 ? m + n : m);
}

void SubHead::setSampleRate(float sr) {
    //... nothing to do
}

void SubHead::setPhase(phase_t phase) {
    phase_ = phase;
    wrIdx_ = wrapBufIndex(static_cast<int>(phase_) + (inc_dir_ * recOffset_));

    // NB: not resetting the resampler here:
    // - it's ok to keep history of input when changing positions.
    // - resamp output doesn't need clearing b/c we write/read from beginning on each sample anyway
}

// softkut patch: any length; `stride` is the distance between consecutive
// frames of this channel in an interleaved buffer (1 for mono).
void SubHead::setBuffer(float *buf, unsigned int frames, unsigned int stride) {
    buf_  = buf;
    stride_ = stride ? stride : 1;
    if (frames != bufFrames_) {
        bufFrames_ = frames;
        // poke() indexes with wrIdx_ before wrapping it: re-wrap for a shorter buffer
        wrIdx_ = wrapBufIndex(static_cast<int>(wrIdx_));
    }
}

void SubHead::setRate(rate_t rate) {
    rate_ = rate;
    inc_dir_ = fsign(rate);
    // NB: resampler doesn't handle negative rates.
    // instead we copy the resampler output backwards into the buffer when rate < 0.
    resamp_.setRate(std::fabs(rate));
}


void SubHead::setState(State state) {
    state_ = state;
    if (state_ == Stopped) {
	fade_ = 0.f;
    }
    if (state == Playing) {
	fade_ = 1.f;
    }
}

void SubHead::setRecOffsetSamples(int d) {
    recOffset_  = d;
}
