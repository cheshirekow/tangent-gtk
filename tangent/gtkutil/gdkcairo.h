#pragma once
// Copyright (C) 2012,2019 Josh Bialkowski (josh.bialkowski@gmail.com)

#include <cairo/cairo.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Sets the source pattern within the Context to an opaque color. This
 * opaque color will then be used for any subsequent drawing operation until
 * a new source pattern is set.
 *
 * The color components are floating point numbers in the range 0 to 1. If
 * the values passed in are outside that range, they will be clamped.
 *
 * @param red red component of color
 * @param green   green component of color
 * @param blue    blue component of color
 *
 * @sa set_source_rgba()
 * @sa set_source()
 */
void cairo_set_source_rgb_str(cairo_t* cr, const char* str);

/** Sets the source pattern within the Context to a translucent color. This
 * color will then be used for any subsequent drawing operation until a new
 * source pattern is set.
 *
 * The color and alpha components are floating point numbers in the range 0
 * to 1. If the values passed in are outside that range, they will be
 * clamped.
 *
 * @param red red component of color
 * @param green   green component of color
 * @param blue    blue component of color
 * @param alpha   alpha component of color
 *
 * @sa set_source_rgb()
 * @sa set_source()
 */
void cairo_set_source_rgba_str(cairo_t* cr, const char* str);

/** Sets the source pattern within the Context to source. This Pattern will
 * then be used for any subsequent drawing operation until a new source
 * pattern is set.
 *
 * Note: The Pattern's transformation matrix will be locked to the user space
 * in effect at the time of set_source(). This means that further
 * modifications of the current transformation matrix will not affect the
 * source pattern.
 *
 * @param source  a Pattern to be used as the source for subsequent drawing
 * operations.
 *
 * @sa Pattern::set_matrix()
 * @sa set_source_rgb()
 * @sa set_source_rgba()
 * @sa set_source(const RefPtr<Surface>& surface, double x, double y)
 */
void cairo_set_source_rgba_gdk(cairo_t* cr, const GdkRGBA* rgba);

/// create a solid pattern from a CSS string
cairo_pattern_t* cairo_pattern_create_rgb_str(const char* str);

/// create a solid pattern from a CSS string
cairo_pattern_t* cairo_pattern_create_rgba_str(const char* str);

/// create a solid pattern from a gdk color spec
cairo_pattern_t* cairo_pattern_create_rgba_gdk(const GdkColor* rgb);

/// create a solid pattern from a gdk color spec
cairo_pattern_t* cairo_pattern_create_rgb_gdk(const GdkRGBA* rgba);

#ifdef __cplusplus
}  // extern "C"
#endif
