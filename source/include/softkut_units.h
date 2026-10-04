// softkut_units.h : message units of the softkut~ shells.
//
// The shells use Max's conventions: times in milliseconds, voices and channels
// counted from 1. The engine uses seconds and 0-based voices. This header holds
// the one conversion between them, with no Max dependency so the offline tests
// cover it.

#ifndef SOFTKUT_UNITS_H
#define SOFTKUT_UNITS_H

#include "softkut_engine.h"

namespace softkut {

// commands whose value is a time (ms in messages, seconds in the engine)
inline bool isTime(CmdId id) {
    switch (id) {
        case CmdId::LoopStart:
        case CmdId::LoopEnd:
        case CmdId::FadeTime:
        case CmdId::Position:
        case CmdId::RecOffset:
        case CmdId::RecPreSlewTime:
        case CmdId::RateSlewTime:
        case CmdId::LevelSlewTime:
        case CmdId::PanSlewTime:
        case CmdId::PhaseQuant:
        case CmdId::PhaseOffset:
        case CmdId::VoiceSync:
            return true;
        default:
            return false;
    }
}

inline float  toEngine(CmdId id, double value) { return static_cast<float>(isTime(id) ? value / 1000.0 : value); }
inline double toUser(CmdId id, double value)   { return isTime(id) ? value * 1000.0 : value; }

// a voice (or inlet) number from a message, counted from 1 -> engine index;
// returns -1 if it is outside 1..count
inline int toIndex(long user, int count) {
    return (user >= 1 && user <= count) ? static_cast<int>(user - 1) : -1;
}

} // namespace softkut

#endif // SOFTKUT_UNITS_H
