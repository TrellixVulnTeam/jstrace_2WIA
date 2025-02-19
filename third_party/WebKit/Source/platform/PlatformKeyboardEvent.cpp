/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/PlatformKeyboardEvent.h"

#if OS(WIN)
#include <windows.h>
#elif OS(MACOSX)
#import <Carbon/Carbon.h>
#endif

namespace blink {

#if OS(WIN)
static const unsigned short HIGHBITMASKSHORT = 0x8000;
#endif

PlatformKeyboardEvent::OverrideCapsLockState PlatformKeyboardEvent::s_overrideCapsLockState =
    PlatformKeyboardEvent::OverrideCapsLockState::Default;

void PlatformKeyboardEvent::disambiguateKeyDownEvent(EventType type)
{
#if OS(WIN)
    // No KeyDown events on Windows to disambiguate.
    ASSERT_NOT_REACHED();
#else
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == PlatformEvent::KeyDown);
    ASSERT(type == PlatformEvent::RawKeyDown || type == PlatformEvent::Char);
    m_type = type;

    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_windowsVirtualKeyCode = 0;
#if OS(MACOSX)
        if (m_text.length() == 1 && (m_text[0U] >= 0xF700 && m_text[0U] <= 0xF7FF)) {
            // According to NSEvents.h, OpenStep reserves the range 0xF700-0xF8FF for function keys. However, some actual private use characters
            // happen to be in this range, e.g. the Apple logo (Option+Shift+K).
            // 0xF7FF is an arbitrary cut-off.
            m_text = String();
            m_unmodifiedText = String();
        }
#endif
    }
#endif
}

PlatformEvent::Modifiers PlatformKeyboardEvent::accessKeyModifiers()
{
    // TODO(crbug.com/618397): Add a settings to control this behavior.
#if OS(MACOSX)
    return static_cast<PlatformEvent::Modifiers>(PlatformEvent::CtrlKey | PlatformEvent::AltKey);
#else
    return PlatformEvent::AltKey;
#endif
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    switch (s_overrideCapsLockState) {
    case OverrideCapsLockState::Default:
#if OS(WIN)
            // FIXME: Does this even work inside the sandbox?
            return GetKeyState(VK_CAPITAL) & 1;
#elif OS(MACOSX)
            return GetCurrentKeyModifiers() & alphaLock;
#else
            NOTIMPLEMENTED();
            return false;
#endif
    case OverrideCapsLockState::On:
        return true;
    case OverrideCapsLockState::Off:
    default:
        return false;
    }
}

PlatformEvent::Modifiers PlatformKeyboardEvent::getCurrentModifierState()
{
    unsigned modifiers = 0;
#if OS(WIN)
    if (GetKeyState(VK_SHIFT) & HIGHBITMASKSHORT)
        modifiers |= ShiftKey;
    if (GetKeyState(VK_CONTROL) & HIGHBITMASKSHORT)
        modifiers |= CtrlKey;
    if (GetKeyState(VK_MENU) & HIGHBITMASKSHORT)
        modifiers |= AltKey;
#elif OS(MACOSX)
    UInt32 currentModifiers = GetCurrentKeyModifiers();
    if (currentModifiers & ::shiftKey)
        modifiers |= ShiftKey;
    if (currentModifiers & ::controlKey)
        modifiers |= CtrlKey;
    if (currentModifiers & ::optionKey)
        modifiers |= AltKey;
    if (currentModifiers & ::cmdKey)
        modifiers |= MetaKey;
#else
    // TODO(crbug.com/538289): Implement on other platforms.
    return static_cast<Modifiers>(0);
#endif
    return static_cast<Modifiers>(modifiers);
}

} // namespace blink
