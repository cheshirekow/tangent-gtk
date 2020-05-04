// Copyright (C) 2012,2019 Josh Bialkowski (josh.bialkowski@gmail.com)
#include "tangent/gtkutil/gdkcairomm.h"

#include <iomanip>
#include <iostream>

#include "tangent/gtkutil/gdkcairo.h"

namespace gdkcairo {

void set_source(const Cairo::RefPtr<Cairo::Context>& ctx,
                const Gdk::RGBA& rgba) {
  ctx->set_source_rgba(rgba.get_red(), rgba.get_green(), rgba.get_blue(),
                       rgba.get_alpha());
}

void set_source_rgb(const Cairo::RefPtr<Cairo::Context>& ctx, const char* str) {
  cairo_set_source_rgb_str(ctx->cobj(), str);
}

void set_source_rgba(const Cairo::RefPtr<Cairo::Context>& ctx,
                     const char* str) {
  cairo_set_source_rgba_str(ctx->cobj(), str);
}

namespace solidpattern {

Cairo::RefPtr<Cairo::SolidPattern> create_rgb(
    const Cairo::RefPtr<Cairo::Context>& ctx, const char* str) {
  cairo_pattern_t* pattern = cairo_pattern_create_rgb_str(str);
  if (!pattern) {
    return Cairo::RefPtr<Cairo::SolidPattern>();
  }
  return Cairo::RefPtr<Cairo::SolidPattern>(
      new Cairo::SolidPattern(pattern, /*has_reference=*/true));
}

Cairo::RefPtr<Cairo::SolidPattern> create_rgba(
    const Cairo::RefPtr<Cairo::Context>& ctx, const char* str) {
  cairo_pattern_t* pattern = cairo_pattern_create_rgba_str(str);
  if (!pattern) {
    return Cairo::RefPtr<Cairo::SolidPattern>();
  }
  return Cairo::RefPtr<Cairo::SolidPattern>(
      new Cairo::SolidPattern(pattern, /*has_reference=*/true));
}

Cairo::RefPtr<Cairo::SolidPattern> create_rgb(
    const Cairo::RefPtr<Cairo::Context>& ctx, const Gdk::Color& rgb) {
  return Cairo::SolidPattern::create_rgb(
      rgb.get_red() / 255.0, rgb.get_green() / 255.0, rgb.get_blue() / 255.0);
}

Cairo::RefPtr<Cairo::SolidPattern> create_rgba(
    const Cairo::RefPtr<Cairo::Context>& ctx, const Gdk::RGBA& rgba) {
  return Cairo::SolidPattern::create_rgba(rgba.get_red(), rgba.get_green(),
                                          rgba.get_blue(), rgba.get_alpha());
}

}  // namespace solidpattern

}  // namespace gdkcairo
