#pragma once
// Copyright (C) 2014,2019 Josh Bialkowski (josh.bialkowski@gmail.com)
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

G_BEGIN_DECLS

#define GTK_TYPE_PANZOOM_AREA (gtk_panzoom_area_get_type())

// Note(josh): the tutorial tells you to do it this way, but honestly the
// macro doesn't do all that much... See e.g. gtkdrawingarea.h
G_DECLARE_DERIVABLE_TYPE(GtkPanZoomArea, gtk_panzoom_area, GTK, PANZOOM_AREA,
                         GtkDrawingArea);

struct _GtkPanZoomAreaClass {
  GtkDrawingAreaClass parent_class;

  /// default signal handler for transformed mouse motion event
  gboolean (*area_motion)(GtkPanZoomArea* area, GdkEventMotion* event);
  /// default signal handler for transformed mouse button event
  gboolean (*area_button)(GtkPanZoomArea* area, GdkEventButton* event);
  /// default signal handler for draw event
  gboolean (*area_draw)(GtkPanZoomArea* area, cairo_t* cr);

  /// padding to add up to 4 new virtual functions without breaking API.
  gpointer padding[4];
};

GtkWidget* gtk_panzoom_area_new();

void gtk_panzoom_area_get_offset(GtkPanZoomArea* area, double out[2]);
void gtk_panzoom_area_set_offset(GtkPanZoomArea* area, double offset[2]);

double gtk_panzoom_area_get_scale(GtkPanZoomArea* area);
void gtk_panzoom_area_set_scale(GtkPanZoomArea* area, double scale);

double gtk_panzoom_area_get_scale_rate(GtkPanZoomArea* area);
void gtk_panzoom_area_set_scale_rate(GtkPanZoomArea* area, double scale_rate);

/// Return the length of the larger dimension of the drawing area (width or
/// height) in pixels.
double gtk_panzoom_area_get_max_dim(GtkPanZoomArea* area);

/// Convert the point (x,y) from GTK coordinates, with the origin at the top
/// left, to a point in traditional cartesian coordinates, with  the origin at
/// the bottom left.
void gtk_panzoom_area_get_rawpoint(GtkPanZoomArea* area, const double in[2],
                                   double out[2]);

/// Return a point int he virtual cartesian plane by appling the offset and
/// scaling of the viewport.
void gtk_panzoom_area_transform_point(GtkPanZoomArea* area, const double in[2],
                                      double out[2]);

/// If true, then the drawing area will draw some shapes so that there is some
/// reference for the pan/zoom features.
void gtk_panzoom_area_set_demodraw(GtkPanZoomArea* area, gboolean enabled);

/// Set the background color used to fill the drawing area prior to emitting
/// the draw-area signal.
void gtk_panzoom_area_set_background_color(GtkPanZoomArea* area,
                                           GdkRGBA* color);

G_END_DECLS

#ifdef __cplusplus
}  // extern "C"
#endif
