// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
#define CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_

#include "base/callback.h"
#include "components/metrics/metrics_service_client.h"

typedef base::Callback<void(bool)> OnMetricsReportingCallbackType;

class PrefService;
class PrefRegistrySimple;

// Changes metrics reporting state without caring about the success of the
// change.
void ChangeMetricsReportingState(bool enabled);

// Changes metrics reporting state to the new value of |enabled|. Starts or
// stops the metrics service based on the new state and then runs |callback_fn|
// (which can be null) with the updated state (as the operation may fail). On
// platforms other than CrOS and Android, also updates the underlying pref.
// TODO(gayane): Support setting the pref on all platforms.
void ChangeMetricsReportingStateWithReply(
    bool enabled,
    const OnMetricsReportingCallbackType& callback_fn);

// Returns whether MetricsReporting can be modified by the user (except
// Android).
bool IsMetricsReportingPolicyManaged();

// Initialize kMetricsReportingEnabled based on kStatsReportingPref device
// setting and add an observer as it is the source of truth on Chrome OS.
void SetupMetricsStateForChromeOS();

#endif  // CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
