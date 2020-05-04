// Copyright (C) 2012,2019 Josh Bialkowski (josh.bialkowski@gmail.com)

#include "tangent/gtkutil/gdkcairo.h"

#include <string.h>

void cairo_set_source_rgba_gdk(cairo_t* cr, const GdkRGBA* rgba) {
  cairo_set_source_rgba(cr, rgba->red, rgba->green, rgba->blue, rgba->alpha);
}

static char parse_color_spec(const char* str, size_t len, unsigned int* spec) {
  size_t expect_strlen = len * 2;
  char valid = TRUE;

  if (strlen(str) == expect_strlen + 1 && str[0] == '#')
    str++;
  else if (strlen(str) != expect_strlen)
    valid = FALSE;

  for (int i = 0; i < len && valid; i++) {
    spec[i] = 0;
    for (int j = 0; j < 2; j++) {
      char c = str[2 * i + j];
      if ('0' <= c && c <= '9')
        spec[i] |= (unsigned char)(c - '0') << 4 * (1 - j);
      else if ('a' <= c && c <= 'f')
        spec[i] |= ((unsigned char)(c - 'a') + 0x0A) << 4 * (1 - j);
      else if ('A' <= c && c <= 'F')
        spec[i] |= ((unsigned char)(c - 'A') + 0x0A) << 4 * (1 - j);
      else
        valid = FALSE;
    }
  }

  return valid;
}

void cairo_set_source_rgb_str(cairo_t* cr, const char* str) {
  unsigned int spec[3];
  char valid = TRUE;

  valid = parse_color_spec(str, 3, spec);
  if (!valid) {
    fprintf(stderr, "Invalid color spec: \"%s\"\n", str);
    return;
  }
  return cairo_set_source_rgb(cr, spec[0] / 255.0, spec[1] / 255.0,
                              spec[2] / 255.0);
}

void cairo_set_source_rgba_str(cairo_t* cr, const char* str) {
  unsigned int spec[4];
  char valid = TRUE;

  valid = parse_color_spec(str, 4, spec);
  if (!valid) {
    fprintf(stderr, "Invalid color spec: \"%s\"\n", str);
    return;
  }
  return cairo_set_source_rgba(cr, spec[0] / 255.0, spec[1] / 255.0,
                               spec[2] / 255.0, spec[3] / 255.0);
}

cairo_pattern_t* cairo_pattern_create_rgb_str(const char* str) {
  unsigned int spec[3];
  char valid = TRUE;

  valid = parse_color_spec(str, 3, spec);
  if (!valid) {
    fprintf(stderr, "Invalid color spec: \"%s\"\n", str);
    return NULL;
  }

  return cairo_pattern_create_rgb(spec[0] / 255.0, spec[1] / 255.0,
                                  spec[2] / 255.0);
}

cairo_pattern_t* cairo_pattern_create_rgba_str(const char* str) {
  unsigned int spec[4];
  char valid = TRUE;

  valid = parse_color_spec(str, 4, spec);
  if (!valid) {
    fprintf(stderr, "Invalid color spec: \"%s\"\n", str);
    return NULL;
  }

  return cairo_pattern_create_rgba(spec[0] / 255.0, spec[1] / 255.0,
                                   spec[2] / 255.0, spec[3] / 255.0);
}

cairo_pattern_t* cairo_pattern_create_rgba_gdk(const GdkColor* rgb) {
  return cairo_pattern_create_rgb(rgb->red, rgb->green, rgb->blue);
}

cairo_pattern_t* cairo_pattern_create_rgb_gdk(const GdkRGBA* rgba) {
  return cairo_pattern_create_rgba(rgba->red, rgba->green, rgba->blue,
                                   rgba->alpha);
}
