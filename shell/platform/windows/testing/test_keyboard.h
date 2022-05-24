// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_WINDOWS_TESTING_TEST_KEYBOARD_H_
#define FLUTTER_SHELL_PLATFORM_WINDOWS_TESTING_TEST_KEYBOARD_H_

#include <windows.h>

#include <functional>
#include <string>

#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/windows/testing/engine_modifier.h"
#include "flutter/shell/platform/windows/testing/wm_builders.h"

#include "gtest/gtest.h"

namespace flutter {
namespace testing {

::testing::AssertionResult _EventEquals(const char* expr_event,
                                        const char* expr_expected,
                                        const FlutterKeyEvent& event,
                                        const FlutterKeyEvent& expected);

// Clone string onto the heap.
//
// If #string is nullptr, returns nullptr. Otherwise, the returned pointer must
// be freed with delete[].
char* clone_string(const char* string);

// Creates a valid Windows LPARAM for WM_KEYDOWN and WM_CHAR from parameters
// given.
//
// While |CreateKeyEventLparam| is flexible, it's recommended to use dedicated
// functions in wm_builders.h, such as |WmKeyDownInfo|.
LPARAM CreateKeyEventLparam(USHORT scancode,
                            bool extended,
                            bool was_down,
                            USHORT repeat_count = 1,
                            bool context_code = 0,
                            bool transition_state = 1);

typedef std::function<bool()> MockKeyEventChannelHandler;
typedef std::function<bool(const FlutterKeyEvent* event)>
    MockKeyEventEmbedderHandler;

void MockEmbedderApiForKeyboard(EngineModifier& modifier,
                                MockKeyEventChannelHandler channel_handler,
                                MockKeyEventEmbedderHandler embedder_handler);

// Simulate a message queue for WM messages.
//
// Subclasses must implement |Win32SendMessage| for how dispatched messages are
// processed.
class MockMessageQueue {
 public:
  // Push a list of messages to the message queue, then dispatch
  // them with |Win32SendMessage| one by one.
  void InjectMessageList(int count, const Win32Message* messages);

  // Peak the next message in the message queue.
  //
  // See Win32's |PeekMessage| for documentation.
  BOOL Win32PeekMessage(LPMSG lpMsg,
                        UINT wMsgFilterMin,
                        UINT wMsgFilterMax,
                        UINT wRemoveMsg);

 protected:
  virtual LRESULT Win32SendMessage(UINT const message,
                                   WPARAM const wparam,
                                   LPARAM const lparam) = 0;

  std::list<Win32Message> _pending_messages;
};

}  // namespace testing
}  // namespace flutter

// Expect the |_target| FlutterKeyEvent has the required properties.
#define EXPECT_EVENT_EQUALS(_target, _type, _physical, _logical, _character, \
                            _synthesized)                                    \
  EXPECT_PRED_FORMAT2(_EventEquals, _target,                                 \
                      (FlutterKeyEvent{                                      \
                          .type = _type,                                     \
                          .physical = _physical,                             \
                          .logical = _logical,                               \
                          .character = _character,                           \
                          .synthesized = _synthesized,                       \
                      }));

#endif  // FLUTTER_SHELL_PLATFORM_WINDOWS_TESTING_TEST_KEYBOARD_H_
