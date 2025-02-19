// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFilterAnimationCurve.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/output/filter_operations.h"
#include "platform/graphics/CompositorFilterOperations.h"

using blink::CompositorFilterKeyframe;

namespace blink {

CompositorFilterAnimationCurve::CompositorFilterAnimationCurve()
    : m_curve(cc::KeyframedFilterAnimationCurve::Create())
{
}

CompositorFilterAnimationCurve::~CompositorFilterAnimationCurve()
{
}

void CompositorFilterAnimationCurve::addLinearKeyframe(const CompositorFilterKeyframe& keyframe)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations, nullptr));

}

void CompositorFilterAnimationCurve::addCubicBezierKeyframe(const CompositorFilterKeyframe& keyframe, const TimingFunction& timingFunction)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations,
        timingFunction.cloneToCC()));
}

void CompositorFilterAnimationCurve::addStepsKeyframe(const CompositorFilterKeyframe& keyframe, const TimingFunction& timingFunction)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations,
        timingFunction.cloneToCC()));
}

void CompositorFilterAnimationCurve::setLinearTimingFunction()
{
    m_curve->SetTimingFunction(nullptr);
}

void CompositorFilterAnimationCurve::setCubicBezierTimingFunction(const TimingFunction& timingFunction)
{
    m_curve->SetTimingFunction(timingFunction.cloneToCC());
}

void CompositorFilterAnimationCurve::setStepsTimingFunction(const TimingFunction& timingFunction)
{
    m_curve->SetTimingFunction(timingFunction.cloneToCC());
}

std::unique_ptr<cc::AnimationCurve> CompositorFilterAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
