// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/flutter_windows_view.h"

#include <comdef.h>
#include <comutil.h>
#include <oleacc.h>

#include <iostream>
#include <vector>

#include "flutter/shell/platform/common/json_message_codec.h"
#include "flutter/shell/platform/embedder/test_utils/proc_table_replacement.h"
#include "flutter/shell/platform/windows/flutter_windows_engine.h"
#include "flutter/shell/platform/windows/flutter_windows_texture_registrar.h"
#include "flutter/shell/platform/windows/testing/engine_modifier.h"
#include "flutter/shell/platform/windows/testing/mock_window_binding_handler.h"
#include "flutter/shell/platform/windows/testing/test_keyboard.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

constexpr uint64_t kScanCodeKeyA = 0x1e;
constexpr uint64_t kVirtualKeyA = 0x41;

namespace {

// A struct to use as a FlutterPlatformMessageResponseHandle so it can keep the
// callbacks and user data passed to the engine's
// PlatformMessageCreateResponseHandle for use in the SendPlatformMessage
// overridden function.
struct TestResponseHandle {
  FlutterDesktopBinaryReply callback;
  void* user_data;
};

static bool test_response = false;

constexpr uint64_t kKeyEventFromChannel = 0x11;
constexpr uint64_t kKeyEventFromEmbedder = 0x22;
static std::vector<int> key_event_logs;

std::unique_ptr<std::vector<uint8_t>> keyHandlingResponse(bool handled) {
  rapidjson::Document document;
  auto& allocator = document.GetAllocator();
  document.SetObject();
  document.AddMember("handled", test_response, allocator);
  return flutter::JsonMessageCodec::GetInstance().EncodeMessage(document);
}

// Returns an engine instance configured with dummy project path values, and
// overridden methods for sending platform messages, so that the engine can
// respond as if the framework were connected.
std::unique_ptr<FlutterWindowsEngine> GetTestEngine() {
  FlutterDesktopEngineProperties properties = {};
  properties.assets_path = L"C:\\foo\\flutter_assets";
  properties.icu_data_path = L"C:\\foo\\icudtl.dat";
  properties.aot_library_path = L"C:\\foo\\aot.so";
  FlutterProjectBundle project(properties);
  auto engine = std::make_unique<FlutterWindowsEngine>(project);

  EngineModifier modifier(engine.get());
  MockEmbedderApiForKeyboard(
      modifier,
      [] {
        key_event_logs.push_back(kKeyEventFromChannel);
        return test_response;
      },
      [](const FlutterKeyEvent* event) {
        key_event_logs.push_back(kKeyEventFromEmbedder);
        return test_response;
      });

  engine->RunWithEntrypoint(nullptr);
  return engine;
}

}  // namespace

TEST(FlutterWindowsViewTest, KeySequence) {
  std::unique_ptr<FlutterWindowsEngine> engine = GetTestEngine();

  test_response = false;

  auto window_binding_handler =
      std::make_unique<::testing::NiceMock<MockWindowBindingHandler>>();
  FlutterWindowsView view(std::move(window_binding_handler));
  view.SetEngine(std::move(engine));

  view.OnKey(kVirtualKeyA, kScanCodeKeyA, WM_KEYDOWN, 'a', false, false);

  EXPECT_EQ(key_event_logs.size(), 2);
  EXPECT_EQ(key_event_logs[0], kKeyEventFromEmbedder);
  EXPECT_EQ(key_event_logs[1], kKeyEventFromChannel);

  key_event_logs.clear();
}

TEST(FlutterWindowsViewTest, RestartClearsKeyboardState) {
  std::unique_ptr<FlutterWindowsEngine> engine = GetTestEngine();

  auto window_binding_handler =
      std::make_unique<::testing::NiceMock<MockWindowBindingHandler>>();
  FlutterWindowsView view(std::move(window_binding_handler));
  view.SetEngine(std::move(engine));

  test_response = false;

  // Receives a KeyA down. Events are dispatched and decided unhandled. Now the
  // keyboard key handler is waiting for the redispatched event.
  view.OnKey(kVirtualKeyA, kScanCodeKeyA, WM_KEYDOWN, 'a', false, false);
  EXPECT_EQ(key_event_logs.size(), 2);
  EXPECT_EQ(key_event_logs[0], kKeyEventFromEmbedder);
  EXPECT_EQ(key_event_logs[1], kKeyEventFromChannel);
  key_event_logs.clear();

  // Resets state so that the keyboard key handler is no longer waiting.
  view.OnPreEngineRestart();

  // Receives another KeyA down. If the state had not been cleared, this event
  // will be considered the redispatched event and ignored.
  view.OnKey(kVirtualKeyA, kScanCodeKeyA, WM_KEYDOWN, 'a', false, false);
  EXPECT_EQ(key_event_logs.size(), 2);
  EXPECT_EQ(key_event_logs[0], kKeyEventFromEmbedder);
  EXPECT_EQ(key_event_logs[1], kKeyEventFromChannel);
  key_event_logs.clear();
}

TEST(FlutterWindowsViewTest, EnableSemantics) {
  std::unique_ptr<FlutterWindowsEngine> engine = GetTestEngine();
  EngineModifier modifier(engine.get());

  bool semantics_enabled = false;
  modifier.embedder_api().UpdateSemanticsEnabled = MOCK_ENGINE_PROC(
      UpdateSemanticsEnabled,
      [&semantics_enabled](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                           bool enabled) {
        semantics_enabled = enabled;
        return kSuccess;
      });

  auto window_binding_handler =
      std::make_unique<::testing::NiceMock<MockWindowBindingHandler>>();
  FlutterWindowsView view(std::move(window_binding_handler));
  view.SetEngine(std::move(engine));

  view.OnUpdateSemanticsEnabled(true);
  EXPECT_TRUE(semantics_enabled);
}

TEST(FlutterWindowsView, AddSemanticsNodeUpdate) {
  std::unique_ptr<FlutterWindowsEngine> engine = GetTestEngine();
  EngineModifier modifier(engine.get());
  modifier.embedder_api().UpdateSemanticsEnabled =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine, bool enabled) {
        return kSuccess;
      };

  auto window_binding_handler =
      std::make_unique<::testing::NiceMock<MockWindowBindingHandler>>();
  FlutterWindowsView view(std::move(window_binding_handler));
  view.SetEngine(std::move(engine));

  // Enable semantics to instantiate accessibility bridge.
  view.OnUpdateSemanticsEnabled(true);

  auto bridge = view.GetEngine()->accessibility_bridge().lock();
  ASSERT_TRUE(bridge);

  // Add root node.
  FlutterSemanticsNode node{sizeof(FlutterSemanticsNode), 0};
  node.label = "name";
  node.value = "value";
  node.platform_view_id = -1;
  bridge->AddFlutterSemanticsNodeUpdate(&node);
  bridge->CommitUpdates();

  // Look up the root windows node delegate.
  auto node_delegate = bridge
                           ->GetFlutterPlatformNodeDelegateFromID(
                               AccessibilityBridge::kRootNodeId)
                           .lock();
  ASSERT_TRUE(node_delegate);
  EXPECT_EQ(node_delegate->GetChildCount(), 0);

  // Get the native IAccessible object.
  IAccessible* native_view = node_delegate->GetNativeViewAccessible();
  ASSERT_TRUE(native_view != nullptr);

  // Property lookups will be made against this node itself.
  VARIANT varchild{};
  varchild.vt = VT_I4;
  varchild.lVal = CHILDID_SELF;

  // Verify node name matches our label.
  BSTR bname = nullptr;
  ASSERT_EQ(native_view->get_accName(varchild, &bname), S_OK);
  std::string name(_com_util::ConvertBSTRToString(bname));
  EXPECT_EQ(name, "name");

  // Verify node value matches.
  BSTR bvalue = nullptr;
  ASSERT_EQ(native_view->get_accValue(varchild, &bvalue), S_OK);
  std::string value(_com_util::ConvertBSTRToString(bvalue));
  EXPECT_EQ(value, "value");

  // Verify node type is static text.
  VARIANT varrole{};
  varrole.vt = VT_I4;
  ASSERT_EQ(native_view->get_accRole(varchild, &varrole), S_OK);
  EXPECT_EQ(varrole.lVal, ROLE_SYSTEM_STATICTEXT);
}

// Verify the native IAccessible COM object tree is an accurate reflection of
// the platform-agnostic tree. Verify both a root node with children as well as
// a non-root node with children, since the AX tree includes special handling
// for the root.
//
//        node0
//        /   \
//    node1    node2
//               |
//             node3
//
// node0 and node2 are grouping nodes. node1 and node2 are static text nodes.
TEST(FlutterWindowsView, AddSemanticsNodeUpdateWithChildren) {
  std::unique_ptr<FlutterWindowsEngine> engine = GetTestEngine();
  EngineModifier modifier(engine.get());
  modifier.embedder_api().UpdateSemanticsEnabled =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine, bool enabled) {
        return kSuccess;
      };

  auto window_binding_handler =
      std::make_unique<::testing::NiceMock<MockWindowBindingHandler>>();
  FlutterWindowsView view(std::move(window_binding_handler));
  view.SetEngine(std::move(engine));

  // Enable semantics to instantiate accessibility bridge.
  view.OnUpdateSemanticsEnabled(true);

  auto bridge = view.GetEngine()->accessibility_bridge().lock();
  ASSERT_TRUE(bridge);

  // Add root node.
  FlutterSemanticsNode node0{sizeof(FlutterSemanticsNode), 0};
  std::vector<int32_t> node0_children{1, 2};
  node0.child_count = node0_children.size();
  node0.children_in_traversal_order = node0_children.data();
  node0.children_in_hit_test_order = node0_children.data();

  FlutterSemanticsNode node1{sizeof(FlutterSemanticsNode), 1};
  node1.label = "prefecture";
  node1.value = "Kyoto";
  FlutterSemanticsNode node2{sizeof(FlutterSemanticsNode), 2};
  std::vector<int32_t> node2_children{3};
  node2.child_count = node2_children.size();
  node2.children_in_traversal_order = node2_children.data();
  node2.children_in_hit_test_order = node2_children.data();
  FlutterSemanticsNode node3{sizeof(FlutterSemanticsNode), 3};
  node3.label = "city";
  node3.value = "Uji";

  bridge->AddFlutterSemanticsNodeUpdate(&node0);
  bridge->AddFlutterSemanticsNodeUpdate(&node1);
  bridge->AddFlutterSemanticsNodeUpdate(&node2);
  bridge->AddFlutterSemanticsNodeUpdate(&node3);
  bridge->CommitUpdates();

  // Look up the root windows node delegate.
  auto node_delegate = bridge
                           ->GetFlutterPlatformNodeDelegateFromID(
                               AccessibilityBridge::kRootNodeId)
                           .lock();
  ASSERT_TRUE(node_delegate);
  EXPECT_EQ(node_delegate->GetChildCount(), 2);

  // Get the native IAccessible object.
  IAccessible* node0_accessible = node_delegate->GetNativeViewAccessible();
  ASSERT_TRUE(node0_accessible != nullptr);

  // Property lookups will be made against this node itself.
  VARIANT varchild{};
  varchild.vt = VT_I4;
  varchild.lVal = CHILDID_SELF;

  // Verify node type is a group.
  VARIANT varrole{};
  varrole.vt = VT_I4;
  ASSERT_EQ(node0_accessible->get_accRole(varchild, &varrole), S_OK);
  EXPECT_EQ(varrole.lVal, ROLE_SYSTEM_GROUPING);

  // Verify child count.
  long node0_child_count = 0;
  ASSERT_EQ(node0_accessible->get_accChildCount(&node0_child_count), S_OK);
  EXPECT_EQ(node0_child_count, 2);

  {
    // Look up first child of node0 (node1), a static text node.
    varchild.lVal = 1;
    IDispatch* node1_dispatch = nullptr;
    ASSERT_EQ(node0_accessible->get_accChild(varchild, &node1_dispatch), S_OK);
    ASSERT_TRUE(node1_dispatch != nullptr);
    IAccessible* node1_accessible = nullptr;
    ASSERT_EQ(node1_dispatch->QueryInterface(
                  IID_IAccessible, reinterpret_cast<void**>(&node1_accessible)),
              S_OK);
    ASSERT_TRUE(node1_accessible != nullptr);

    // Verify node name matches our label.
    varchild.lVal = CHILDID_SELF;
    BSTR bname = nullptr;
    ASSERT_EQ(node1_accessible->get_accName(varchild, &bname), S_OK);
    std::string name(_com_util::ConvertBSTRToString(bname));
    EXPECT_EQ(name, "prefecture");

    // Verify node value matches.
    BSTR bvalue = nullptr;
    ASSERT_EQ(node1_accessible->get_accValue(varchild, &bvalue), S_OK);
    std::string value(_com_util::ConvertBSTRToString(bvalue));
    EXPECT_EQ(value, "Kyoto");

    // Verify node type is static text.
    VARIANT varrole{};
    varrole.vt = VT_I4;
    ASSERT_EQ(node1_accessible->get_accRole(varchild, &varrole), S_OK);
    EXPECT_EQ(varrole.lVal, ROLE_SYSTEM_STATICTEXT);

    // Verify the parent node is the root.
    IDispatch* parent_dispatch;
    node1_accessible->get_accParent(&parent_dispatch);
    IAccessible* parent_accessible;
    ASSERT_EQ(
        parent_dispatch->QueryInterface(
            IID_IAccessible, reinterpret_cast<void**>(&parent_accessible)),
        S_OK);
    EXPECT_EQ(parent_accessible, node0_accessible);
  }

  // Look up second child of node0 (node2), a parent group for node3.
  varchild.lVal = 2;
  IDispatch* node2_dispatch = nullptr;
  ASSERT_EQ(node0_accessible->get_accChild(varchild, &node2_dispatch), S_OK);
  ASSERT_TRUE(node2_dispatch != nullptr);
  IAccessible* node2_accessible = nullptr;
  ASSERT_EQ(node2_dispatch->QueryInterface(
                IID_IAccessible, reinterpret_cast<void**>(&node2_accessible)),
            S_OK);
  ASSERT_TRUE(node2_accessible != nullptr);

  {
    // Verify child count.
    long node2_child_count = 0;
    ASSERT_EQ(node2_accessible->get_accChildCount(&node2_child_count), S_OK);
    EXPECT_EQ(node2_child_count, 1);

    // Verify node type is static text.
    varchild.lVal = CHILDID_SELF;
    VARIANT varrole{};
    varrole.vt = VT_I4;
    ASSERT_EQ(node2_accessible->get_accRole(varchild, &varrole), S_OK);
    EXPECT_EQ(varrole.lVal, ROLE_SYSTEM_GROUPING);

    // Verify the parent node is the root.
    IDispatch* parent_dispatch;
    node2_accessible->get_accParent(&parent_dispatch);
    IAccessible* parent_accessible;
    ASSERT_EQ(
        parent_dispatch->QueryInterface(
            IID_IAccessible, reinterpret_cast<void**>(&parent_accessible)),
        S_OK);
    EXPECT_EQ(parent_accessible, node0_accessible);
  }

  {
    // Look up only child of node2 (node3), a static text node.
    varchild.lVal = 1;
    IDispatch* node3_dispatch = nullptr;
    ASSERT_EQ(node2_accessible->get_accChild(varchild, &node3_dispatch), S_OK);
    ASSERT_TRUE(node3_dispatch != nullptr);
    IAccessible* node3_accessible = nullptr;
    ASSERT_EQ(node3_dispatch->QueryInterface(
                  IID_IAccessible, reinterpret_cast<void**>(&node3_accessible)),
              S_OK);
    ASSERT_TRUE(node3_accessible != nullptr);

    // Verify node name matches our label.
    varchild.lVal = CHILDID_SELF;
    BSTR bname = nullptr;
    ASSERT_EQ(node3_accessible->get_accName(varchild, &bname), S_OK);
    std::string name(_com_util::ConvertBSTRToString(bname));
    EXPECT_EQ(name, "city");

    // Verify node value matches.
    BSTR bvalue = nullptr;
    ASSERT_EQ(node3_accessible->get_accValue(varchild, &bvalue), S_OK);
    std::string value(_com_util::ConvertBSTRToString(bvalue));
    EXPECT_EQ(value, "Uji");

    // Verify node type is static text.
    VARIANT varrole{};
    varrole.vt = VT_I4;
    ASSERT_EQ(node3_accessible->get_accRole(varchild, &varrole), S_OK);
    EXPECT_EQ(varrole.lVal, ROLE_SYSTEM_STATICTEXT);

    // Verify the parent node is node2.
    IDispatch* parent_dispatch;
    node3_accessible->get_accParent(&parent_dispatch);
    IAccessible* parent_accessible;
    ASSERT_EQ(
        parent_dispatch->QueryInterface(
            IID_IAccessible, reinterpret_cast<void**>(&parent_accessible)),
        S_OK);
    EXPECT_EQ(parent_accessible, node2_accessible);
  }
}

// Verify the native IAccessible accHitTest method returns the correct
// IAccessible COM object for the given coordinates.
//
//                         +-----------+
//                         |     |     |
//        node0            |     |  B  |
//        /   \            |  A  |-----|
//    node1    node2       |     |  C  |
//               |         |     |     |
//             node3       +-----------+
//
// node0 and node2 are grouping nodes. node1 and node2 are static text nodes.
//
// node0 is located at 0,0 with size 500x500. It spans areas A, B, and C.
// node1 is located at 0,0 with size 250x500. It spans area A.
// node2 is located at 250,0 with size 250x500. It spans areas B and C.
// node3 is located at 250,250 with size 250x250. It spans area C.
TEST(FlutterWindowsViewTest, AccessibilityHitTesting) {
  constexpr FlutterTransformation kIdentityTransform = {1, 0, 0,  //
                                                        0, 1, 0,  //
                                                        0, 0, 1};

  std::unique_ptr<FlutterWindowsEngine> engine = GetTestEngine();
  EngineModifier modifier(engine.get());
  modifier.embedder_api().UpdateSemanticsEnabled =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine, bool enabled) {
        return kSuccess;
      };

  auto window_binding_handler =
      std::make_unique<::testing::NiceMock<MockWindowBindingHandler>>();
  FlutterWindowsView view(std::move(window_binding_handler));
  view.SetEngine(std::move(engine));

  // Enable semantics to instantiate accessibility bridge.
  view.OnUpdateSemanticsEnabled(true);

  auto bridge = view.GetEngine()->accessibility_bridge().lock();
  ASSERT_TRUE(bridge);

  // Add root node at origin. Size 500x500.
  FlutterSemanticsNode node0{sizeof(FlutterSemanticsNode), 0};
  std::vector<int32_t> node0_children{1, 2};
  node0.rect = {0, 0, 500, 500};
  node0.transform = kIdentityTransform;
  node0.child_count = node0_children.size();
  node0.children_in_traversal_order = node0_children.data();
  node0.children_in_hit_test_order = node0_children.data();

  // Add node 1 located at 0,0 relative to node 0. Size 250x500.
  FlutterSemanticsNode node1{sizeof(FlutterSemanticsNode), 1};
  node1.rect = {0, 0, 250, 500};
  node1.transform = kIdentityTransform;
  node1.label = "prefecture";
  node1.value = "Kyoto";

  // Add node 2 located at 250,0 relative to node 0. Size 250x500.
  FlutterSemanticsNode node2{sizeof(FlutterSemanticsNode), 2};
  std::vector<int32_t> node2_children{3};
  node2.rect = {0, 0, 250, 500};
  node2.transform = {1, 0, 250, 0, 1, 0, 0, 0, 1};
  node2.child_count = node2_children.size();
  node2.children_in_traversal_order = node2_children.data();
  node2.children_in_hit_test_order = node2_children.data();

  // Add node 3 located at 0,250 relative to node 2. Size 250, 250.
  FlutterSemanticsNode node3{sizeof(FlutterSemanticsNode), 3};
  node3.rect = {0, 0, 250, 250};
  node3.transform = {1, 0, 0, 0, 1, 250, 0, 0, 1};
  node3.label = "city";
  node3.value = "Uji";

  bridge->AddFlutterSemanticsNodeUpdate(&node0);
  bridge->AddFlutterSemanticsNodeUpdate(&node1);
  bridge->AddFlutterSemanticsNodeUpdate(&node2);
  bridge->AddFlutterSemanticsNodeUpdate(&node3);
  bridge->CommitUpdates();

  // Look up the root windows node delegate.
  auto node0_delegate = bridge->GetFlutterPlatformNodeDelegateFromID(0).lock();
  ASSERT_TRUE(node0_delegate);
  auto node1_delegate = bridge->GetFlutterPlatformNodeDelegateFromID(1).lock();
  ASSERT_TRUE(node1_delegate);
  auto node2_delegate = bridge->GetFlutterPlatformNodeDelegateFromID(2).lock();
  ASSERT_TRUE(node2_delegate);
  auto node3_delegate = bridge->GetFlutterPlatformNodeDelegateFromID(3).lock();
  ASSERT_TRUE(node3_delegate);

  // Get the native IAccessible root object.
  IAccessible* node0_accessible = node0_delegate->GetNativeViewAccessible();
  ASSERT_TRUE(node0_accessible != nullptr);

  // Perform a hit test that should hit node 1.
  VARIANT varchild{};
  ASSERT_TRUE(SUCCEEDED(node0_accessible->accHitTest(150, 150, &varchild)));
  EXPECT_EQ(varchild.vt, VT_DISPATCH);
  EXPECT_EQ(varchild.pdispVal, node1_delegate->GetNativeViewAccessible());

  // Perform a hit test that should hit node 2.
  varchild = {};
  ASSERT_TRUE(SUCCEEDED(node0_accessible->accHitTest(450, 150, &varchild)));
  EXPECT_EQ(varchild.vt, VT_DISPATCH);
  EXPECT_EQ(varchild.pdispVal, node2_delegate->GetNativeViewAccessible());

  // Perform a hit test that should hit node 3.
  varchild = {};
  ASSERT_TRUE(SUCCEEDED(node0_accessible->accHitTest(450, 450, &varchild)));
  EXPECT_EQ(varchild.vt, VT_DISPATCH);
  EXPECT_EQ(varchild.pdispVal, node3_delegate->GetNativeViewAccessible());
}

}  // namespace testing
}  // namespace flutter
