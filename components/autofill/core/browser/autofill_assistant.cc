// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_assistant.h"

#include "base/containers/adapters.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_constants.h"

namespace autofill {

AutofillAssistant::AutofillAssistant(AutofillManager* autofill_manager)
    : credit_card_form_data_(nullptr),
      autofill_manager_(autofill_manager),
      weak_ptr_factory_(this) {}

AutofillAssistant::~AutofillAssistant() {}

void AutofillAssistant::Reset() {
  credit_card_form_data_.reset();
}

bool AutofillAssistant::CanShowCreditCardAssist(
    const std::vector<FormStructure*>& form_structures) {
  if (!IsAutofillCreditCardAssistEnabled() || credit_card_form_data_)
    return false;

  for (FormStructure* cur_form : base::Reversed(form_structures)) {
    if (cur_form->IsCompleteCreditCardForm()) {
      credit_card_form_data_.reset(new FormData(cur_form->ToFormData()));
      break;
    }
  }
  return !!credit_card_form_data_;
}

void AutofillAssistant::ShowAssistForCreditCard(const CreditCard& card) {
  DCHECK(credit_card_form_data_);
  autofill_manager_->client()->ConfirmCreditCardFillAssist(
      card, base::Bind(&AutofillAssistant::OnUserDidAcceptCreditCardFill,
                       weak_ptr_factory_.GetWeakPtr(), card));
}

void AutofillAssistant::OnUserDidAcceptCreditCardFill(const CreditCard& card) {
  // TODO(crbug.com/630656): Trigger CVC dialog flow for card filling.
  autofill_manager_->FillCreditCardForm(kNoQueryId, *credit_card_form_data_,
                                        credit_card_form_data_->fields[0], card,
                                        base::string16());
}

}  // namespace autofill
