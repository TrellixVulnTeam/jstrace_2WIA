/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TimingCalculations_h
#define TimingCalculations_h

#include "core/animation/AnimationEffect.h"
#include "core/animation/Timing.h"
#include "platform/animation/AnimationUtilities.h"
#include "wtf/MathExtras.h"

namespace blink {

static inline double multiplyZeroAlwaysGivesZero(double x, double y)
{
    ASSERT(!isNull(x));
    ASSERT(!isNull(y));
    return x && y ? x * y : 0;
}

static inline AnimationEffect::Phase calculatePhase(double activeDuration, double localTime, const Timing& specified)
{
    ASSERT(activeDuration >= 0);
    if (isNull(localTime))
        return AnimationEffect::PhaseNone;
    double endTime = specified.startDelay + activeDuration + specified.endDelay;
    if (localTime < std::min(specified.startDelay, endTime))
        return AnimationEffect::PhaseBefore;
    if (localTime >= std::min(specified.startDelay + activeDuration, endTime))
        return AnimationEffect::PhaseAfter;
    return AnimationEffect::PhaseActive;
}

static inline bool isActiveInParentPhase(AnimationEffect::Phase parentPhase, Timing::FillMode fillMode)
{
    switch (parentPhase) {
    case AnimationEffect::PhaseBefore:
        return fillMode == Timing::FillModeBackwards || fillMode == Timing::FillModeBoth;
    case AnimationEffect::PhaseActive:
        return true;
    case AnimationEffect::PhaseAfter:
        return fillMode == Timing::FillModeForwards || fillMode == Timing::FillModeBoth;
    default:
        NOTREACHED();
        return false;
    }
}

static inline double calculateActiveTime(double activeDuration, Timing::FillMode fillMode, double localTime, AnimationEffect::Phase parentPhase, AnimationEffect::Phase phase, const Timing& specified)
{
    ASSERT(activeDuration >= 0);
    ASSERT(phase == calculatePhase(activeDuration, localTime, specified));

    switch (phase) {
    case AnimationEffect::PhaseBefore:
        if (fillMode == Timing::FillModeBackwards || fillMode == Timing::FillModeBoth)
            return 0;
        return nullValue();
    case AnimationEffect::PhaseActive:
        if (isActiveInParentPhase(parentPhase, fillMode))
            return localTime - specified.startDelay;
        return nullValue();
    case AnimationEffect::PhaseAfter:
        if (fillMode == Timing::FillModeForwards || fillMode == Timing::FillModeBoth)
            return std::max(0.0, std::min(activeDuration, activeDuration + specified.endDelay));
        return nullValue();
    case AnimationEffect::PhaseNone:
        ASSERT(isNull(localTime));
        return nullValue();
    default:
        NOTREACHED();
        return nullValue();
    }
}

static inline double calculateScaledActiveTime(double activeDuration, double activeTime, double startOffset, const Timing& specified)
{
    ASSERT(activeDuration >= 0);
    ASSERT(startOffset >= 0);

    if (isNull(activeTime))
        return nullValue();

    ASSERT(activeTime >= 0 && activeTime <= activeDuration);

    if (specified.playbackRate == 0)
        return startOffset;

    if (!std::isfinite(activeTime))
        return std::numeric_limits<double>::infinity();

    return multiplyZeroAlwaysGivesZero(specified.playbackRate < 0 ? activeTime - activeDuration : activeTime, specified.playbackRate) + startOffset;
}

static inline bool endsOnIterationBoundary(double iterationCount, double iterationStart)
{
    ASSERT(std::isfinite(iterationCount));
    return !fmod(iterationCount + iterationStart, 1);
}

// TODO(crbug.com/630915): Align this function with current Web Animations spec text.
static inline double calculateIterationTime(double iterationDuration, double repeatedDuration, double scaledActiveTime, double startOffset, AnimationEffect::Phase phase, const Timing& specified)
{
    ASSERT(iterationDuration > 0);
    ASSERT(repeatedDuration == multiplyZeroAlwaysGivesZero(iterationDuration, specified.iterationCount));

    if (isNull(scaledActiveTime))
        return nullValue();

    ASSERT(scaledActiveTime >= 0);
    ASSERT(scaledActiveTime <= repeatedDuration + startOffset);

    if (!std::isfinite(scaledActiveTime)
        || (scaledActiveTime - startOffset == repeatedDuration && specified.iterationCount && endsOnIterationBoundary(specified.iterationCount, specified.iterationStart)))
        return iterationDuration;

    ASSERT(std::isfinite(scaledActiveTime));
    double iterationTime = fmod(scaledActiveTime, iterationDuration);

    // This implements step 3 of
    // http://w3c.github.io/web-animations/#calculating-the-simple-iteration-progress
    if (iterationTime == 0
        && phase == AnimationEffect::PhaseAfter
        && repeatedDuration != 0
        && scaledActiveTime != 0)
        return iterationDuration;

    return iterationTime;
}

static inline double calculateCurrentIteration(double iterationDuration, double iterationTime, double scaledActiveTime, const Timing& specified)
{
    ASSERT(iterationDuration > 0);
    ASSERT(isNull(iterationTime) || iterationTime >= 0);

    if (isNull(scaledActiveTime))
        return nullValue();

    ASSERT(iterationTime >= 0);
    ASSERT(iterationTime <= iterationDuration);
    ASSERT(scaledActiveTime >= 0);

    if (!scaledActiveTime)
        return 0;

    if (iterationTime == iterationDuration)
        return specified.iterationStart + specified.iterationCount - 1;

    return floor(scaledActiveTime / iterationDuration);
}

static inline double calculateDirectedTime(double currentIteration, double iterationDuration, double iterationTime, const Timing& specified)
{
    ASSERT(isNull(currentIteration) || currentIteration >= 0);
    ASSERT(iterationDuration > 0);

    if (isNull(iterationTime))
        return nullValue();

    ASSERT(currentIteration >= 0);
    ASSERT(iterationTime >= 0);
    ASSERT(iterationTime <= iterationDuration);

    const bool currentIterationIsOdd = fmod(currentIteration, 2) >= 1;
    const bool currentDirectionIsForwards = specified.direction == Timing::PlaybackDirectionNormal
        || (specified.direction == Timing::PlaybackDirectionAlternate && !currentIterationIsOdd)
        || (specified.direction == Timing::PlaybackDirectionAlternateReverse && currentIterationIsOdd);

    return currentDirectionIsForwards ? iterationTime : iterationDuration - iterationTime;
}

static inline double calculateTransformedTime(double currentIteration, double iterationDuration, double iterationTime, const Timing& specified)
{
    ASSERT(isNull(currentIteration) || currentIteration >= 0);
    ASSERT(iterationDuration > 0);
    ASSERT(isNull(iterationTime) || (iterationTime >= 0 && iterationTime <= iterationDuration));

    double directedTime = calculateDirectedTime(currentIteration, iterationDuration, iterationTime, specified);
    if (isNull(directedTime))
        return nullValue();
    if (!std::isfinite(iterationDuration))
        return directedTime;
    double timeFraction = directedTime / iterationDuration;
    ASSERT(timeFraction >= 0 && timeFraction <= 1);
    return multiplyZeroAlwaysGivesZero(iterationDuration, specified.timingFunction->evaluate(timeFraction, accuracyForDuration(iterationDuration)));
}

} // namespace blink

#endif
