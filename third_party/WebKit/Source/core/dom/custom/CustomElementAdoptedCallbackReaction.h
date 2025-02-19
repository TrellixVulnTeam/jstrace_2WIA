// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementAdoptedCallbackReaction_h
#define CustomElementAdoptedCallbackReaction_h

#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementReaction.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT CustomElementAdoptedCallbackReaction final
    : public CustomElementReaction {
    WTF_MAKE_NONCOPYABLE(CustomElementAdoptedCallbackReaction);
public:
    CustomElementAdoptedCallbackReaction(CustomElementDefinition*);

private:
    void invoke(Element*) override;
};

} // namespace blink

#endif // CustomElementAdoptedCallbackReaction_h
