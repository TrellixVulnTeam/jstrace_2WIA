// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_dialog.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "grit/components_strings.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using views::GridLayout;

namespace {

void InitCrashReporterIfEnabled(bool enabled) {
  if (enabled)
    breakpad::InitCrashReporter(std::string());
}

}  // namespace

namespace first_run {

bool ShowFirstRunDialog(Profile* profile) {
  return FirstRunDialog::Show(profile);
}

}  // namespace first_run

// static
bool FirstRunDialog::Show(Profile* profile) {
  bool dialog_shown = false;

#if defined(GOOGLE_CHROME_BUILD)
  // If the metrics reporting is managed, we won't ask.
  if (!IsMetricsReportingPolicyManaged()) {
    FirstRunDialog* dialog = new FirstRunDialog(profile);
    views::DialogDelegate::CreateDialogWidget(dialog, NULL, NULL)->Show();

    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
    base::RunLoop run_loop;
    dialog->quit_runloop_ = run_loop.QuitClosure();
    run_loop.Run();
    dialog_shown = true;
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  return dialog_shown;
}

FirstRunDialog::FirstRunDialog(Profile* profile)
    : profile_(profile),
      make_default_(NULL),
      report_crashes_(NULL) {
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int related_y = views::kRelatedControlVerticalSpacing;

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  make_default_ = new views::Checkbox(l10n_util::GetStringUTF16(
      IDS_FR_CUSTOMIZE_DEFAULT_BROWSER));
  make_default_->SetChecked(true);
  layout->AddView(make_default_);

  layout->StartRowWithPadding(0, 0, 0, related_y);
  report_crashes_ = new views::Checkbox(l10n_util::GetStringUTF16(
      IDS_OPTIONS_ENABLE_LOGGING));
  // Having this box checked means the user has to opt-out of metrics recording.
  report_crashes_->SetChecked(!first_run::IsMetricsReportingOptIn());
  layout->AddView(report_crashes_);
}

FirstRunDialog::~FirstRunDialog() {
}

void FirstRunDialog::Done() {
  CHECK(!quit_runloop_.is_null());
  quit_runloop_.Run();
}

views::View* FirstRunDialog::CreateExtraView() {
  views::Link* link = new views::Link(l10n_util::GetStringUTF16(
      IDS_LEARN_MORE));
  link->set_listener(this);
  return link;
}

bool FirstRunDialog::Accept() {
  GetWidget()->Hide();

  ChangeMetricsReportingStateWithReply(report_crashes_->checked(),
                                       base::Bind(&InitCrashReporterIfEnabled));

  if (make_default_->checked())
    shell_integration::SetAsDefaultBrowser();

  Done();
  return true;
}

int FirstRunDialog::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

void FirstRunDialog::WindowClosing() {
  first_run::SetShouldShowWelcomePage();
  Done();
}

void FirstRunDialog::LinkClicked(views::Link* source, int event_flags) {
  platform_util::OpenExternal(profile_, GURL(chrome::kLearnMoreReportingURL));
}
