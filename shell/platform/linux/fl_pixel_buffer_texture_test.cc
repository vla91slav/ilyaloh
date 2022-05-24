// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux/public/flutter_linux/fl_pixel_buffer_texture.h"
#include "flutter/shell/platform/linux/fl_pixel_buffer_texture_private.h"
#include "flutter/shell/platform/linux/fl_texture_private.h"
#include "flutter/shell/platform/linux/fl_texture_registrar_private.h"
#include "flutter/shell/platform/linux/public/flutter_linux/fl_texture_registrar.h"
#include "flutter/shell/platform/linux/testing/fl_test.h"
#include "gtest/gtest.h"

#include <epoxy/gl.h>

static constexpr uint32_t BUFFER_WIDTH = 4u;
static constexpr uint32_t BUFFER_HEIGHT = 4u;
static constexpr uint32_t REAL_BUFFER_WIDTH = 2u;
static constexpr uint32_t REAL_BUFFER_HEIGHT = 2u;

G_DECLARE_FINAL_TYPE(FlTestPixelBufferTexture,
                     fl_test_pixel_buffer_texture,
                     FL,
                     TEST_PIXEL_BUFFER_TEXTURE,
                     FlPixelBufferTexture)

/// A simple texture with fixed contents.
struct _FlTestPixelBufferTexture {
  FlPixelBufferTexture parent_instance;
};

G_DEFINE_TYPE(FlTestPixelBufferTexture,
              fl_test_pixel_buffer_texture,
              fl_pixel_buffer_texture_get_type())

static gboolean fl_test_pixel_buffer_texture_copy_pixels(
    FlPixelBufferTexture* texture,
    const uint8_t** out_buffer,
    uint32_t* width,
    uint32_t* height,
    GError** error) {
  EXPECT_TRUE(FL_IS_TEST_PIXEL_BUFFER_TEXTURE(texture));

  // RGBA
  static const uint8_t buffer[] = {0x0a, 0x1a, 0x2a, 0x3a, 0x4a, 0x5a,
                                   0x6a, 0x7a, 0x8a, 0x9a, 0xaa, 0xba,
                                   0xca, 0xda, 0xea, 0xfa};
  EXPECT_EQ(*width, BUFFER_WIDTH);
  EXPECT_EQ(*height, BUFFER_HEIGHT);
  *out_buffer = buffer;
  *width = REAL_BUFFER_WIDTH;
  *height = REAL_BUFFER_HEIGHT;

  return TRUE;
}

static void fl_test_pixel_buffer_texture_class_init(
    FlTestPixelBufferTextureClass* klass) {
  FL_PIXEL_BUFFER_TEXTURE_CLASS(klass)->copy_pixels =
      fl_test_pixel_buffer_texture_copy_pixels;
}

static void fl_test_pixel_buffer_texture_init(FlTestPixelBufferTexture* self) {}

static FlTestPixelBufferTexture* fl_test_pixel_buffer_texture_new() {
  return FL_TEST_PIXEL_BUFFER_TEXTURE(
      g_object_new(fl_test_pixel_buffer_texture_get_type(), nullptr));
}

// Test that getting the texture ID works.
TEST(FlPixelBufferTextureTest, TextureID) {
  // Texture ID is not assigned until the pixel buffer is copied once.
  g_autoptr(FlTexture) texture = FL_TEXTURE(fl_test_pixel_buffer_texture_new());
  EXPECT_EQ(fl_texture_get_texture_id(texture),
            reinterpret_cast<int64_t>(texture));
}

// Test that populating an OpenGL texture works.
TEST(FlPixelBufferTextureTest, PopulateTexture) {
  g_autoptr(FlPixelBufferTexture) texture =
      FL_PIXEL_BUFFER_TEXTURE(fl_test_pixel_buffer_texture_new());
  FlutterOpenGLTexture opengl_texture = {0};
  g_autoptr(GError) error = nullptr;
  EXPECT_TRUE(fl_pixel_buffer_texture_populate(
      texture, BUFFER_WIDTH, BUFFER_HEIGHT, &opengl_texture, &error));
  EXPECT_EQ(error, nullptr);
  EXPECT_EQ(opengl_texture.width, REAL_BUFFER_WIDTH);
  EXPECT_EQ(opengl_texture.height, REAL_BUFFER_HEIGHT);
}
