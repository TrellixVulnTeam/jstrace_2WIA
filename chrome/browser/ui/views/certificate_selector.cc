// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/certificate_selector.h"

#include <stddef.h>  // For size_t.
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#endif

namespace chrome {

const int CertificateSelector::kTableViewWidth = 500;
const int CertificateSelector::kTableViewHeight = 150;

class CertificateSelector::CertificateTableModel : public ui::TableModel {
 public:
  // |certs| and |provider_names| must have the same size.
  CertificateTableModel(const net::CertificateList& certs,
                        const std::vector<std::string>& provider_names);

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int index, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

 private:
  struct Row {
    base::string16 subject;
    base::string16 issuer;
    base::string16 provider;
    base::string16 serial;
  };
  std::vector<Row> rows_;

  DISALLOW_COPY_AND_ASSIGN(CertificateTableModel);
};

CertificateSelector::CertificateTableModel::CertificateTableModel(
    const net::CertificateList& certs,
    const std::vector<std::string>& provider_names) {
  DCHECK_EQ(certs.size(), provider_names.size());
  for (size_t i = 0; i < certs.size(); i++) {
    net::X509Certificate* cert = certs[i].get();
    Row row;
    row.subject = base::UTF8ToUTF16(cert->subject().GetDisplayName());
    row.issuer = base::UTF8ToUTF16(cert->issuer().GetDisplayName());
    row.provider = base::UTF8ToUTF16(provider_names[i]);
    if (cert->serial_number().size() < std::numeric_limits<size_t>::max() / 2) {
      row.serial = base::UTF8ToUTF16(base::HexEncode(
          cert->serial_number().data(), cert->serial_number().size()));
    }
    rows_.push_back(row);
  }
}

int CertificateSelector::CertificateTableModel::RowCount() {
  return rows_.size();
}

base::string16 CertificateSelector::CertificateTableModel::GetText(
    int index,
    int column_id) {
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), rows_.size());

  const Row& row = rows_[index];
  switch (column_id) {
    case IDS_CERT_SELECTOR_SUBJECT_COLUMN:
      return row.subject;
    case IDS_CERT_SELECTOR_ISSUER_COLUMN:
      return row.issuer;
    case IDS_CERT_SELECTOR_PROVIDER_COLUMN:
      return row.provider;
    case IDS_CERT_SELECTOR_SERIAL_COLUMN:
      return row.serial;
    default:
      NOTREACHED();
  }
  return base::string16();
}

void CertificateSelector::CertificateTableModel::SetObserver(
    ui::TableModelObserver* observer) {}

CertificateSelector::CertificateSelector(
    const net::CertificateList& certificates,
    content::WebContents* web_contents)
    : web_contents_(web_contents), table_(nullptr), view_cert_button_(nullptr) {
  CHECK(web_contents_);

  // |provider_names| and |certificates_| are parallel arrays.
  // The entry at index |i| is the provider name for |certificates_[i]|.
  std::vector<std::string> provider_names;
#if defined(OS_CHROMEOS)
  chromeos::CertificateProviderService* service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistryFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());

  for (const auto& cert : certificates) {
    std::string provider_name;
    bool has_extension = false;
    std::string extension_id;
    if (service->LookUpCertificate(*cert, &has_extension, &extension_id)) {
      if (!has_extension) {
        // This certificate was provided by an extension but isn't provided by
        // any extension currently. Don't expose it to the user.
        continue;
      }
      const auto* extension = extension_registry->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED);
      if (!extension) {
        // This extension was unloaded in the meantime. Don't show the
        // certificate.
        continue;
      }
      provider_name = extension->short_name();
      show_provider_column_ = true;
    }  // Otherwise the certificate is provided by the platform.

    certificates_.push_back(cert);
    provider_names.push_back(provider_name);
  }
#else
  provider_names.assign(certificates.size(), std::string());
  certificates_ = certificates;
#endif

  model_.reset(new CertificateTableModel(certificates_, provider_names));
}

CertificateSelector::~CertificateSelector() {
  table_->SetModel(nullptr);
}

// static
bool CertificateSelector::CanShow(content::WebContents* web_contents) {
  // GetTopLevelWebContents returns |web_contents| if it is not a guest.
  content::WebContents* top_level_web_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(web_contents);
  return web_modal::WebContentsModalDialogManager::FromWebContents(
             top_level_web_contents) != nullptr;
}

void CertificateSelector::Show() {
  constrained_window::ShowWebModalDialogViews(this, web_contents_);

  // Select the first row automatically.  This must be done after the dialog has
  // been created.
  table_->Select(0);
}

void CertificateSelector::InitWithText(
    std::unique_ptr<views::View> text_label) {
  views::GridLayout* const layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int kColumnSetId = 0;
  views::ColumnSet* const column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(text_label.release());

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn(IDS_CERT_SELECTOR_SUBJECT_COLUMN,
                                    ui::TableColumn::LEFT, -1, 0.4f));
  columns.push_back(ui::TableColumn(IDS_CERT_SELECTOR_ISSUER_COLUMN,
                                    ui::TableColumn::LEFT, -1, 0.2f));
  if (show_provider_column_) {
    columns.push_back(ui::TableColumn(IDS_CERT_SELECTOR_PROVIDER_COLUMN,
                                      ui::TableColumn::LEFT, -1, 0.4f));
  }
  columns.push_back(ui::TableColumn(IDS_CERT_SELECTOR_SERIAL_COLUMN,
                                    ui::TableColumn::LEFT, -1, 0.2f));
  table_ = new views::TableView(model_.get(), columns, views::TEXT_ONLY,
                                true /* single_selection */);
  table_->SetObserver(this);
  layout->StartRow(1, kColumnSetId);
  layout->AddView(table_->CreateParentIfNecessary(), 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  kTableViewWidth, kTableViewHeight);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

ui::TableModel* CertificateSelector::table_model_for_testing() const {
  return model_.get();
}

net::X509Certificate* CertificateSelector::GetSelectedCert() const {
  const int selected = table_->FirstSelectedRow();
  if (selected < 0)  // Nothing is selected in |table_|.
    return nullptr;
  CHECK_LT(static_cast<size_t>(selected), certificates_.size());
  return certificates_[selected].get();
}

bool CertificateSelector::CanResize() const {
  return true;
}

base::string16 CertificateSelector::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CLIENT_CERT_DIALOG_TITLE);
}

bool CertificateSelector::IsDialogButtonEnabled(ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK || GetSelectedCert() != nullptr;
}

views::View* CertificateSelector::GetInitiallyFocusedView() {
  DCHECK(table_);
  return table_;
}

views::View* CertificateSelector::CreateExtraView() {
  DCHECK(!view_cert_button_);
  view_cert_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_PAGEINFO_CERT_INFO_BUTTON));
  return view_cert_button_;
}

ui::ModalType CertificateSelector::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void CertificateSelector::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == view_cert_button_) {
    net::X509Certificate* const cert = GetSelectedCert();
    if (cert)
      ShowCertificateViewer(web_contents_,
                            web_contents_->GetTopLevelNativeWindow(), cert);
  }
}

void CertificateSelector::OnSelectionChanged() {
  GetDialogClientView()->ok_button()->SetEnabled(GetSelectedCert() != nullptr);
}

void CertificateSelector::OnDoubleClick() {
  if (GetSelectedCert())
    GetDialogClientView()->AcceptWindow();
}

}  // namespace chrome
