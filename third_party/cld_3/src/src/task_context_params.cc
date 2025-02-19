/* Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// This file contains the hard-coded parameters from the training workflow. If
// you update the binary model, you may need to update the variables below as
// well.

#include "third_party/cld_3/src/src/task_context_params.h"

#include "third_party/cld_3/src/src/task_context.h"

namespace chrome_lang_id {

void TaskContextParams::ToTaskContext(TaskContext *context) {
  context->SetParameter("language_identifier_features",
                        kLanguageIdentifierFeatures);
  context->SetParameter("language_identifier_embedding_names",
                        kLanguageIdentifierEmbeddingNames);
  context->SetParameter("language_identifier_embedding_dims",
                        kLanguageIdentifierEmbeddingDims);
}

int TaskContextParams::GetNumLanguages() {
  int i = 0;
  while (kLanguageNames[i] != nullptr) {
    i++;
  }
  return i;
}

const char *const TaskContextParams::kLanguageNames[] = {
    "eo", "eu", "ta", "de", "mt", "te", "su", "uz", "ne", "nl", "sw", "sq",
    "hmn", "ja", "no", "mn", "so", "ko", "th", "kk", "sl", "ig", "mr", "zu",
    "ml", "hr", "bs", "lo", "cy", "hy", "uk", "pt", "yi", "lv", "iw", "cs",
    "vi", "jv", "be", "km", "mk", "tr", "zh", "da", "sv", "fi", "ht", "af",
    "la", "id", "ca", "el", "ka", "sr", "it", "sk", "ru", "bg", "ny", "fa",
    "gl", "et", "ms", "is", "ur", "ha", "mi", "hi", "bn", "fr", "hu", "my",
    "tg", "ro", "ar", "st", "kn", "az", "si", "mg", "fil", "en", "gu", "es",
    "pl", "ga", "lt", "yo", "ceb", "pa",

    // last element must be nullptr
    nullptr,
};

const char TaskContextParams::kLanguageIdentifierFeatures[] = R"(
continuous-bag-of-ngrams(include_terminators=true,
include_spaces=false,use_equal_weight=false,id_dim=1000,size=2);
continuous-bag-of-ngrams(include_terminators=true,
include_spaces=false,use_equal_weight=false,id_dim=5000,size=4);
continuous-bag-of-ngrams(include_terminators=true,
include_spaces=false,use_equal_weight=false,id_dim=5000,size=3);
continuous-bag-of-ngrams(include_terminators=true,
include_spaces=false,use_equal_weight=false,id_dim=100,size=1)
)";

const char TaskContextParams::kLanguageIdentifierEmbeddingNames[] =
    "bigrams;quadgrams;trigrams;unigrams";

const char TaskContextParams::kLanguageIdentifierEmbeddingDims[] =
    "16;16;16;16";
}  // namespace chrome_lang_id
