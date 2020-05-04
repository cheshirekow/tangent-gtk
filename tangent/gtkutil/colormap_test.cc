// Copyright 2018 Josh Bialkowski <josh.bialkowski@gmail.com>
#include <gtest/gtest.h>

#include "tangent/gtkutil/colormap.h"

TEST(ColorMap, TestKnownColorMap) {
  colormap::Map3u map = colormap::get_map(colormap::ACCENT);
  EXPECT_EQ(map.get_nsupports(), 8);
  EXPECT_EQ(map.get_range().min, 0);
  EXPECT_EQ(map.get_range().max, 8);
  auto color0 = map(0);
  auto color1 = map(1);
  EXPECT_EQ(color0.r, 0x7f);
  EXPECT_EQ(color0.g, 0xc9);
  EXPECT_EQ(color0.b, 0x7f);
  EXPECT_EQ(color1.r, 0xbe);
  EXPECT_EQ(color1.g, 0xae);
  EXPECT_EQ(color1.b, 0xd4);
}
