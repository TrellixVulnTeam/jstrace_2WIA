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

#ifndef THIRD_PARTY_CLD_3_SRC_SRC_EMBEDDING_FEATURE_EXTRACTOR_H_
#define THIRD_PARTY_CLD_3_SRC_SRC_EMBEDDING_FEATURE_EXTRACTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "third_party/cld_3/src/src/feature_extractor.h"
#include "third_party/cld_3/src/src/task_context.h"
#include "third_party/cld_3/src/src/workspace.h"

namespace chrome_lang_id {

// An EmbeddingFeatureExtractor manages the extraction of features for
// embedding-based models. It wraps a sequence of underlying classes of feature
// extractors, along with associated predicate maps. Each class of feature
// extractors is associated with a name, e.g., "unigrams", "bigrams".
//
// The class is split between a generic abstract version,
// GenericEmbeddingFeatureExtractor (that can be initialized without knowing the
// signature of the ExtractFeatures method) and a typed version.
//
// The predicate maps must be initialized before use: they can be loaded using
// Read() or updated via UpdateMapsForExample.
class GenericEmbeddingFeatureExtractor {
 public:
  GenericEmbeddingFeatureExtractor();
  virtual ~GenericEmbeddingFeatureExtractor();

  // Get the prefix string to put in front of all arguments, so they don't
  // conflict with other embedding models.
  virtual const string ArgPrefix() const = 0;

  // Sets up predicate maps and embedding space names that are common for all
  // embedding based feature extractors.
  virtual void Setup(TaskContext *context);
  virtual void Init(TaskContext *context);

  // Requests workspace for the underlying feature extractors. This is
  // implemented in the typed class.
  virtual void RequestWorkspaces(WorkspaceRegistry *registry) = 0;

  // Number of predicates for the embedding at a given index (vocabulary size.)
  int EmbeddingSize(int index) const {
    return generic_feature_extractor(index).GetDomainSize();
  }

  // Returns number of embedding spaces.
  int NumEmbeddings() const { return embedding_dims_.size(); }

  // Returns the number of features in the embedding space.
  int FeatureSize(int idx) const {
    return generic_feature_extractor(idx).feature_types();
  }

  // Returns the dimensionality of the embedding space.
  int EmbeddingDims(int index) const { return embedding_dims_[index]; }

  // Accessor for embedding dims (dimensions of the embedding spaces).
  const vector<int> &embedding_dims() const { return embedding_dims_; }

  const vector<string> &embedding_fml() const { return embedding_fml_; }

  // Get parameter name by concatenating the prefix and the original name.
  string GetParamName(const string &param_name) const {
    string name = ArgPrefix();
    name += "_";
    name += param_name;
    return name;
  }

 protected:
  // Provides the generic class with access to the templated extractors. This is
  // used to get the type information out of the feature extractor without
  // knowing the specific calling arguments of the extractor itself.
  virtual const GenericFeatureExtractor &generic_feature_extractor(
      int idx) const = 0;

 private:
  // Embedding space names for parameter sharing.
  vector<string> embedding_names_;

  // FML strings for each feature extractor.
  vector<string> embedding_fml_;

  // Size of each of the embedding spaces (maximum predicate id).
  vector<int> embedding_sizes_;

  // Embedding dimensions of the embedding spaces (i.e. 32, 64 etc.)
  vector<int> embedding_dims_;

  // Whether or not to add string descriptions to converted examples.
  bool add_strings_;
};

// Templated, object-specific implementation of the
// EmbeddingFeatureExtractor. EXTRACTOR should be a FeatureExtractor<OBJ,
// ARGS...> class that has the appropriate FeatureTraits() to ensure that
// locator type features work.
//
// Note: for backwards compatibility purposes, this always reads the FML spec
// from "<prefix>_features".
template <class EXTRACTOR, class OBJ, class... ARGS>
class EmbeddingFeatureExtractor : public GenericEmbeddingFeatureExtractor {
 public:
  // Sets up all predicate maps, feature extractors, and flags.
  void Setup(TaskContext *context) override {
    GenericEmbeddingFeatureExtractor::Setup(context);
    feature_extractors_.resize(embedding_fml().size());
    for (size_t i = 0; i < embedding_fml().size(); ++i) {
      feature_extractors_[i].Parse(embedding_fml()[i]);
      feature_extractors_[i].Setup(context);
    }
  }

  // Initializes resources needed by the feature extractors.
  void Init(TaskContext *context) override {
    GenericEmbeddingFeatureExtractor::Init(context);
    for (auto &feature_extractor : feature_extractors_) {
      feature_extractor.Init(context);
    }
  }

  // Requests workspaces from the registry. Must be called after Init(), and
  // before Preprocess().
  void RequestWorkspaces(WorkspaceRegistry *registry) override {
    for (auto &feature_extractor : feature_extractors_) {
      feature_extractor.RequestWorkspaces(registry);
    }
  }

  // Must be called on the object one state for each sentence, before any
  // feature extraction (e.g., UpdateMapsForExample, ExtractSparseFeatures).
  void Preprocess(WorkspaceSet *workspaces, OBJ *obj) const {
    for (auto &feature_extractor : feature_extractors_) {
      feature_extractor.Preprocess(workspaces, obj);
    }
  }

  // Extracts features using the extractors. Note that features must already
  // be initialized to the correct number of feature extractors. No predicate
  // mapping is applied.
  void ExtractFeatures(const WorkspaceSet &workspaces, const OBJ &obj,
                       ARGS... args, vector<FeatureVector> *features) const {
    for (size_t i = 0; i < feature_extractors_.size(); ++i) {
      features->at(i).clear();
      feature_extractors_.at(i).ExtractFeatures(workspaces, obj, args...,
                                                &features->at(i));
    }
  }

 protected:
  // Provides generic access to the feature extractors.
  const GenericFeatureExtractor &generic_feature_extractor(
      int idx) const override {
    return feature_extractors_.at(idx);
  }

 private:
  // Templated feature extractor class.
  vector<EXTRACTOR> feature_extractors_;
};

}  // namespace chrome_lang_id

#endif  // THIRD_PARTY_CLD_3_SRC_SRC_EMBEDDING_FEATURE_EXTRACTOR_H_
