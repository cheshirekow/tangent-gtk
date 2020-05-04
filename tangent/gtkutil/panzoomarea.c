// Copyright (C) 2014,2019 Josh Bialkowski (josh.bialkowski@gmail.com)
#include "tangent/gtkutil/panzoomarea.h"

#include <cairo/cairo-gobject.h>
#include <math.h>

#include "tangent/gtkutil/gdkcairo.h"

// =============================================================================
//  Stolen from GTK internals
// =============================================================================

#ifdef ENABLE_NLS
#define P_(String) g_dgettext(GETTEXT_PACKAGE "-properties", String)
#else
#define P_(String) (String)
#endif

/* not really I18N-related, but also a string marker macro */
#define I_(string) g_intern_static_string(string)

// NOTE(josh): for some reason this is hidden within gtk??
static gboolean boolean_handled_accumulator(GSignalInvocationHint* ihint,
                                            GValue* return_accu,
                                            const GValue* handler_return,
                                            gpointer dummy) {
  gboolean continue_emission;
  gboolean signal_handled;

  signal_handled = g_value_get_boolean(handler_return);
  g_value_set_boolean(return_accu, signal_handled);
  continue_emission = !signal_handled;

  return continue_emission;
}

// =============================================================================
//  Private members
// =============================================================================
typedef struct _GtkPanZoomAreaPrivate {
  GtkAdjustment* offset_x;  ///< offset of the viewport
  GtkAdjustment* offset_y;  ///< offset of the viewport
  GtkAdjustment* scale;  ///< size (in virtual units) of the largest dimension
                         ///< of the widget
  GtkAdjustment* scale_rate;  ///< when the mouse wheel is turned, multiply or
                              ///< divide the scale by this much
  gdouble last_pos[2];        ///< The last mouse position (used during pan)
  gboolean active;  ///< if true, controls are enabled. Othrwise, they are
                    ///< disabled and events pass through.
  gint pan_button;  ///< which mouse button is used for pan
  guint pan_button_mask;
  gboolean demo_draw_enabled;
  GdkRGBA bg_color;
} GtkPanZoomAreaPrivate;

// =============================================================================
//  Type definition
// =============================================================================

G_DEFINE_TYPE_WITH_PRIVATE(GtkPanZoomArea, gtk_panzoom_area,
                           GTK_TYPE_DRAWING_AREA);

// =============================================================================
//  Properties
// =============================================================================

enum {
  PROP_OFFSET_X_ADJUSTMENT = 1,
  PROP_OFFSET_Y_ADJUSTMENT,
  PROP_SCALE_ADJUSTMENT,
  PROP_SCALE_RATE_ADJUSTMENT,
  PROP_ACTIVE,
  PROP_PAN_BUTTON,
  PROP_DEMO_DRAW_ENABLED,
  N_PROPERTIES
};

static GParamSpec* obj_properties[N_PROPERTIES] = {NULL};

// =============================================================================
//  Signals
// =============================================================================

enum {
  SIGNO_AREA_MOTION,
  SIGNO_AREA_BUTTON,
  SIGNO_AREA_DRAW,
  N_SIGNALS,
};

static guint widget_signals[N_SIGNALS] = {0};

// =============================================================================
//  Virtual Function Overrides
// =============================================================================

// GObject overrides
static void gtk_panzoom_area_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec);
static void gtk_panzoom_area_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec);
static void gtk_panzoom_area_finalize(GObject* gobject);
static void gtk_panzoom_area_dispose(GObject* gobject);

// GtkWidget overrides
static gboolean gtk_panzoom_area_motion_notify_event(GtkWidget* widget,
                                                     GdkEventMotion* event);
static gboolean gtk_panzoom_area_button_press_event(GtkWidget* widget,
                                                    GdkEventButton* event);
static gboolean gtk_panzoom_area_button_release_event(GtkWidget* widget,
                                                      GdkEventButton* event);
static gboolean gtk_panzoom_area_scroll_event(GtkWidget* widget,
                                              GdkEventScroll* event);
static gboolean gtk_panzoom_area_draw(GtkWidget* widget, cairo_t* cr);
static void gtk_panzoom_area_destroy(GtkWidget* widget);

gboolean gtk_panzoom_area_sig_motion(GtkPanZoomArea* area,
                                     GdkEventMotion* event) {
  return FALSE;
}

gboolean gtk_panzoom_area_sig_button(GtkPanZoomArea* area,
                                     GdkEventButton* event) {
  return FALSE;
}

gboolean gtk_panzoom_area_sig_draw(GtkPanZoomArea* this, cairo_t* cr) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  if (!priv->demo_draw_enabled) {
    return FALSE;
  }

  cairo_scale(cr, 0.5, 0.5);
  cairo_translate(cr, 0.5, 0.5);
  cairo_set_line_width(cr, 0.01);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  // Green Traignle
  cairo_move_to(cr, 0.1, 0.1);
  cairo_line_to(cr, 0.9, 0.1);
  cairo_line_to(cr, 0.5, 0.9);
  cairo_line_to(cr, 0.1, 0.1);
  cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.5);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);
  // Red Square
  cairo_move_to(cr, 0.5, 0.3);
  cairo_line_to(cr, 1.2, 0.3);
  cairo_line_to(cr, 1.2, 1.0);
  cairo_line_to(cr, 0.5, 1.0);
  cairo_line_to(cr, 0.5, 0.3);
  cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.5);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);
  // Blue Circle
  cairo_move_to(cr, 1.1, 0.1);
  cairo_arc(cr, 0.7, 0.1, 0.4, 0, 2 * M_PI);
  cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.5);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);

  return FALSE;
}

// =============================================================================
//  Constructors
// =============================================================================

// https://developer.gnome.org/gobject/stable/howto-gobject-construction.html
static void gtk_panzoom_area_class_init(GtkPanZoomAreaClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);

  // ----------------------------
  //  Override virtual functions
  // ----------------------------

  // GtkPanZoomArea methods
  klass->area_motion = gtk_panzoom_area_sig_motion;
  klass->area_button = gtk_panzoom_area_sig_button;
  klass->area_draw = gtk_panzoom_area_sig_draw;

  // Override property GObject methods
  GObjectClass* object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gtk_panzoom_area_set_property;
  object_class->get_property = gtk_panzoom_area_get_property;
  object_class->finalize = gtk_panzoom_area_finalize;
  object_class->dispose = gtk_panzoom_area_dispose;

  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->motion_notify_event = gtk_panzoom_area_motion_notify_event;
  widget_class->button_press_event = gtk_panzoom_area_button_press_event;
  widget_class->button_release_event = gtk_panzoom_area_button_release_event;
  widget_class->scroll_event = gtk_panzoom_area_scroll_event;
  widget_class->draw = gtk_panzoom_area_draw;
  widget_class->destroy = gtk_panzoom_area_destroy;

  // ---------------------------
  //  Define/Install Properties
  // ---------------------------

  obj_properties[PROP_OFFSET_X_ADJUSTMENT] =
      g_param_spec_object("offset-x-adjustment", "Offset (x)",
                          "Adjustment defining the x-offset of the viewport",
                          GTK_TYPE_ADJUSTMENT, G_PARAM_READWRITE);
  obj_properties[PROP_OFFSET_Y_ADJUSTMENT] =
      g_param_spec_object("offset-y-adjustment", "Offset (y)",
                          "Adjustment defining the y-offset of the viewport",
                          GTK_TYPE_ADJUSTMENT, G_PARAM_READWRITE);
  obj_properties[PROP_SCALE_ADJUSTMENT] = g_param_spec_object(
      "scale-adjustment", "Scale",
      "Adjustment defining the scale/zoom level of the viewport by defining "
      "the length (in virtual units) of the longest edge.",
      GTK_TYPE_ADJUSTMENT, G_PARAM_READWRITE);
  obj_properties[PROP_SCALE_RATE_ADJUSTMENT] = g_param_spec_object(
      "scale-rate-adjustment", "Scale rate",
      "Adjustment defining the multiplier used when changing the scale in "
      "response to a button press. Zoom-in will multiply the scale by this "
      "value. Zoom-out will divide.",
      GTK_TYPE_ADJUSTMENT, G_PARAM_READWRITE);
  obj_properties[PROP_ACTIVE] =
      g_param_spec_boolean("active", "Active",
                           "If true, captures mouse events for pan/zoom. If "
                           "false, mouse events are passed through.",
                           TRUE, G_PARAM_READWRITE);
  obj_properties[PROP_PAN_BUTTON] = g_param_spec_int(
      "pan-button", "Pan Button", "Which mouse button to monitor for pan tool",
      0, 4, 3, G_PARAM_READWRITE);

  obj_properties[PROP_DEMO_DRAW_ENABLED] = g_param_spec_boolean(
      "demo-draw-enabled", "Enable Demo Draw",
      "If true, then the default area-draw signal handler will draw some "
      "shapes so that there is a point of reference for pan/zoom actions.",
      FALSE, G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

  // ------------------------
  //  Define/Install Signals
  // ------------------------
  // G_STRUCT_OFFSET(GtkPanZoomAreaClass, foobarfun);
  widget_signals[SIGNO_AREA_MOTION] = g_signal_new(
      I_("area-motion"), G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(GtkPanZoomAreaClass, area_motion),
      boolean_handled_accumulator, NULL, NULL, G_TYPE_BOOLEAN, 1,
      GDK_TYPE_EVENT);
  widget_signals[SIGNO_AREA_BUTTON] = g_signal_new(
      I_("area-button"), G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(GtkPanZoomAreaClass, area_button),
      boolean_handled_accumulator, NULL, NULL, G_TYPE_BOOLEAN, 1,
      GDK_TYPE_EVENT);

  // NOTE(josh): see gtk_widget_draw_marshaller which wraps the callback in
  // a cairo save/restore to prevent any transient modifications. We could
  // copy that and use ithere if we want, but then we need to figure out how
  // to generate the internal marshaller. See e.g.
  // https://github.com/GNOME/gtk/blob/gtk-3-24/gtk/gtkwidget.c#L934
  // and
  // https://developer.gnome.org/gobject/stable/glib-genmarshal.html
  // for more details
  widget_signals[SIGNO_AREA_DRAW] = g_signal_new(
      I_("area-draw"), G_TYPE_FROM_CLASS(gobject_class), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(GtkPanZoomAreaClass, area_draw),
      boolean_handled_accumulator, NULL, NULL, G_TYPE_BOOLEAN, 1,
      CAIRO_GOBJECT_TYPE_CONTEXT);
}

static void gtk_panzoom_area_init(GtkPanZoomArea* area) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(area);
  // initialize everything to default values. They are already zero initialized
  priv->offset_x = gtk_adjustment_new(0, -1e6, 1e6, 1, 10, 10);
  priv->offset_y = gtk_adjustment_new(0, -1e6, 1e6, 1, 10, 10);
  priv->scale = gtk_adjustment_new(1.0, 0.01, 1e6, 1, 10, 10);
  priv->scale_rate = gtk_adjustment_new(2.0, 0.01, 1e4, 1, 10, 10);
  priv->active = TRUE;
  priv->pan_button = 3;
  priv->pan_button_mask = GDK_BUTTON3_MASK;
  priv->demo_draw_enabled = FALSE;
  gdk_rgba_parse(&priv->bg_color, "#FFFFFF");

  GtkWidget* widget = GTK_WIDGET(area);

  gint events = GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK |
                GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                GDK_SCROLL_MASK;
  gtk_widget_add_events(widget, events);
}

// =============================================================================
//  Non-virtual Function Implementations
// =============================================================================

void gtk_panzoom_area_get_offset(GtkPanZoomArea* this, double out[2]) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  if (priv->offset_x) {
    out[0] = gtk_adjustment_get_value(priv->offset_x);
  } else {
    out[0] = 0;
  }
  if (priv->offset_x) {
    out[1] = gtk_adjustment_get_value(priv->offset_y);
  } else {
    out[1] = 0;
  }
}
void gtk_panzoom_area_set_offset(GtkPanZoomArea* this, double offset[2]) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  if (priv->offset_x) {
    gtk_adjustment_set_value(priv->offset_x, offset[0]);
  }
  if (priv->offset_x) {
    gtk_adjustment_set_value(priv->offset_y, offset[1]);
  }
}

double gtk_panzoom_area_get_scale(GtkPanZoomArea* this) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  if (priv->scale) {
    return gtk_adjustment_get_value(priv->scale);
  }
  return 1;
}
void gtk_panzoom_area_set_scale(GtkPanZoomArea* this, double scale) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  if (priv->scale) {
    gtk_adjustment_set_value(priv->scale, scale);
  }
}

double gtk_panzoom_area_get_scale_rate(GtkPanZoomArea* this) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  if (priv->scale_rate) {
    return gtk_adjustment_get_value(priv->scale_rate);
  } else {
    return 1;
  }
}

void gtk_panzoom_area_set_scale_rate(GtkPanZoomArea* this, double scale_rate) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  if (priv->scale_rate && scale_rate > 1e-6) {
    gtk_adjustment_set_value(priv->scale_rate, scale_rate);
  }
}

/// Return the length of the larger dimension of the drawing area (width or
/// height) in pixels.
double gtk_panzoom_area_get_max_dim(GtkPanZoomArea* this) {
  GtkWidget* widget = GTK_WIDGET(this);
  double width = gtk_widget_get_allocated_width(widget);
  double height = gtk_widget_get_allocated_height(widget);
  return fmax(width, height);
}

/// Convert the point (x,y) from GTK coordinates, with the origin at the top
/// left, to a point in traditional cartesian coordinates, with  the origin at
/// the bottom left.
void gtk_panzoom_area_get_rawpoint(GtkPanZoomArea* this, const double in[2],
                                   double out[2]) {
  GtkWidget* widget = GTK_WIDGET(this);
  out[0] = in[0];
  out[1] = gtk_widget_get_allocated_height(widget) - in[1];
}

/// Return a point in the virtual cartesian plane by appling the offset and
/// scaling of the viewport.
void gtk_panzoom_area_transform_point(GtkPanZoomArea* this, const double in[2],
                                      double out[2]) {
  double scale = gtk_panzoom_area_get_scale(this);
  double maxdim = gtk_panzoom_area_get_max_dim(this);
  double offset[2] = {0, 0};
  double rawpoint[2] = {0, 0};
  gtk_panzoom_area_get_offset(this, offset);
  gtk_panzoom_area_get_rawpoint(this, in, rawpoint);

  for (size_t idx = 0; idx < 2; idx++) {
    out[idx] = offset[idx] + (rawpoint[idx] * (scale / maxdim));
  }
}

void gtk_panzoom_area_set_demodraw(GtkPanZoomArea* this, gboolean enabled) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  priv->demo_draw_enabled = enabled;
}

void gtk_panzoom_area_set_background_color(GtkPanZoomArea* this,
                                           GdkRGBA* color) {
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  priv->bg_color = *color;
}

GtkWidget* gtk_panzoom_area_new() {
  return GTK_WIDGET(g_object_new(GTK_TYPE_PANZOOM_AREA, NULL));
}

// =============================================================================
//  Virtual Function Implementations
// =============================================================================

// helper function to assign an adjustment property
static void set_adjustment(GtkAdjustment** field, const GValue* value) {
  if (!value) {
    fprintf(stderr, "WARNING: assigning a null adjustment\n");
    return;
  }

  GObject* object = g_value_get_object(value);
  if (!object) {
    fprintf(stderr, "WARNING: assigned a not-object\n");
    return;
  }

  GtkAdjustment* adjustment = GTK_ADJUSTMENT(object);
  if (!adjustment) {
    fprintf(stderr, "WARNING: assigned a not-adjustment: %s\n",
            G_OBJECT_TYPE_NAME(object));
    return;
  }

  if (*field) {
    g_object_unref(G_OBJECT(*field));
  }
  *field = adjustment;
  g_object_ref(G_OBJECT(*field));
}

static void gtk_panzoom_area_set_property(GObject* object, guint property_id,
                                          const GValue* value,
                                          GParamSpec* pspec) {
  GtkPanZoomArea* this = GTK_PANZOOM_AREA(object);
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  switch (property_id) {
    case PROP_OFFSET_X_ADJUSTMENT: {
      set_adjustment(&(priv->offset_x), value);
      break;
    }

    case PROP_OFFSET_Y_ADJUSTMENT: {
      set_adjustment(&(priv->offset_y), value);
      break;
    }
    case PROP_SCALE_ADJUSTMENT: {
      set_adjustment(&(priv->scale), value);
      break;
    }
    case PROP_SCALE_RATE_ADJUSTMENT: {
      set_adjustment(&(priv->scale_rate), value);
      break;
    }

    case PROP_ACTIVE: {
      priv->active = g_value_get_boolean(value);
      break;
    }
    case PROP_PAN_BUTTON: {
      priv->pan_button = g_value_get_int(value);
      priv->pan_button_mask = (1 << (7 + priv->pan_button));
      break;
    }

    case PROP_DEMO_DRAW_ENABLED: {
      priv->demo_draw_enabled = g_value_get_boolean(value);
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void gtk_panzoom_area_get_property(GObject* object, guint property_id,
                                          GValue* value, GParamSpec* pspec) {
  GtkPanZoomArea* this = GTK_PANZOOM_AREA(object);
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);
  switch (property_id) {
    case PROP_OFFSET_X_ADJUSTMENT: {
      g_value_set_object(value, priv->offset_x);
      break;
    }
    case PROP_OFFSET_Y_ADJUSTMENT: {
      g_value_set_object(value, priv->offset_y);
      break;
    }
    case PROP_SCALE_ADJUSTMENT: {
      g_value_set_object(value, priv->scale);
      break;
    }
    case PROP_SCALE_RATE_ADJUSTMENT: {
      g_value_set_object(value, priv->scale_rate);
      break;
    }
    case PROP_ACTIVE: {
      g_value_set_boolean(value, priv->active);
      break;
    }
    case PROP_PAN_BUTTON: {
      g_value_set_int(value, priv->pan_button);
      break;
    }

    case PROP_DEMO_DRAW_ENABLED: {
      g_value_set_int(value, priv->demo_draw_enabled);
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void gtk_panzoom_area_finalize(GObject* gobject) {
  // Free any non-reference counted stuff here
  G_OBJECT_CLASS(gtk_panzoom_area_parent_class)->finalize(gobject);
}

// TODO(josh): see
// https://wiki.gnome.org/HowDoI/CustomWidgets
// which recommends override widget_class->destroy instead
static void gtk_panzoom_area_dispose(GObject* gobject) {
  G_OBJECT_CLASS(gtk_panzoom_area_parent_class)->dispose(gobject);
}

static gboolean gtk_panzoom_area_motion_notify_event(GtkWidget* widget,
                                                     GdkEventMotion* event) {
  GtkPanZoomArea* this = GTK_PANZOOM_AREA(widget);
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);

  // Re-emit the event via the transformed signal.
  GTK_WIDGET_CLASS(gtk_panzoom_area_parent_class)
      ->motion_notify_event(widget, event);
  GdkEventMotion transformed = *event;
  gtk_panzoom_area_transform_point(this, &event->x, &transformed.x);
  gboolean handled_by_transformed = FALSE;
  g_signal_emit(widget, widget_signals[SIGNO_AREA_MOTION], 0, &transformed,
                &handled_by_transformed);
  if (handled_by_transformed) {
    // If it was handled by a signal subscriber, then return
    gtk_widget_queue_draw(widget);
    return TRUE;
  }

  if (event->state & priv->pan_button_mask) {
    double offset[2] = {0, 0};  //< current offset
    double loc[2] = {0, 0};  //< current mouse location, in normalized cartesian
                             // coordinates with origin in the bottom left

    double new_offset[2] = {0, 0};  //< new offset of the viewport
    double scale = gtk_panzoom_area_get_scale(this);
    double maxdim = gtk_panzoom_area_get_max_dim(this);

    gtk_panzoom_area_get_rawpoint(this, &event->x, loc);
    gtk_panzoom_area_get_offset(this, offset);

    for (size_t idx = 0; idx < 2; idx++) {
      // The (x or y) relative change in mouse pointer coordinate
      double delta = loc[idx] - priv->last_pos[idx];
      // Translate the offset by the scaled amount that the mouse hase moved
      new_offset[idx] = offset[idx] - (scale / maxdim) * delta;
      // Update the last observed mouse position
      priv->last_pos[idx] = loc[idx];
    }
    gtk_panzoom_area_set_offset(this, new_offset);
    gtk_widget_queue_draw(widget);
    return TRUE;
  }

  return FALSE;
}

static gboolean gtk_panzoom_area_button_press_event(GtkWidget* widget,
                                                    GdkEventButton* event) {
  GtkPanZoomArea* this = GTK_PANZOOM_AREA(widget);
  GtkPanZoomAreaPrivate* priv =
      gtk_panzoom_area_get_instance_private(GTK_PANZOOM_AREA(widget));

  GTK_WIDGET_CLASS(gtk_panzoom_area_parent_class)
      ->button_press_event(widget, event);
  GdkEventButton transformed = *event;
  gtk_panzoom_area_transform_point(this, &event->x, &transformed.x);
  gboolean handled_by_transformed = FALSE;
  g_signal_emit(widget, widget_signals[SIGNO_AREA_BUTTON], 0, &transformed,
                &handled_by_transformed);
  if (handled_by_transformed) {
    // If it was handled by a signal subscriber, then return
    gtk_widget_queue_draw(widget);
    return TRUE;
  }

  if (event->button == priv->pan_button) {
    gtk_panzoom_area_get_rawpoint(this, &event->x, priv->last_pos);
    gtk_widget_queue_draw(widget);
    return TRUE;
  }

  return FALSE;
}

static gboolean gtk_panzoom_area_button_release_event(GtkWidget* widget,
                                                      GdkEventButton* event) {
  GtkPanZoomArea* this = GTK_PANZOOM_AREA(widget);
  GTK_WIDGET_CLASS(gtk_panzoom_area_parent_class)
      ->button_press_event(widget, event);
  GdkEventButton transformed = *event;
  gtk_panzoom_area_transform_point(this, &event->x, &transformed.x);
  return TRUE;
}

static gboolean gtk_panzoom_area_scroll_event(GtkWidget* widget,
                                              GdkEventScroll* event) {
  GtkPanZoomArea* this = GTK_PANZOOM_AREA(widget);
  GtkPanZoomAreaPrivate* priv =
      gtk_panzoom_area_get_instance_private(GTK_PANZOOM_AREA(widget));

  // after the change in scale, we want the mouse pointer to be over
  // the same location in the scaled view
  double rawpoint[2] = {0, 0};
  double centerpoint[2] = {0, 0};
  gtk_panzoom_area_get_rawpoint(this, &event->x, rawpoint);
  gtk_panzoom_area_transform_point(this, &event->x, centerpoint);

  if (event->direction == GDK_SCROLL_UP) {
    if (priv->scale) {
      double current_value = gtk_adjustment_get_value(priv->scale);
      double scale_rate = gtk_panzoom_area_get_scale_rate(this);
      gtk_adjustment_set_value(priv->scale, current_value * scale_rate);
    }
  } else if (event->direction == GDK_SCROLL_DOWN) {
    if (priv->scale) {
      double current_value = gtk_adjustment_get_value(priv->scale);
      double scale_rate = gtk_panzoom_area_get_scale_rate(this);
      gtk_adjustment_set_value(priv->scale, current_value / scale_rate);
    }
  } else {
    GtkWidgetClass* widget_class =
        GTK_WIDGET_CLASS(gtk_panzoom_area_parent_class);
    if (widget_class->scroll_event) {
      return widget_class->scroll_event(widget, event);
    } else {
      return TRUE;
    }
  }

  // Compute the viewpoint delta that will keep the current viewport
  // center.
  double scale = gtk_panzoom_area_get_scale(this);
  double maxdim = gtk_panzoom_area_get_max_dim(this);
  double offset[2] = {0, 0};
  for (size_t idx = 0; idx < 2; idx++) {
    offset[idx] = centerpoint[idx] - (rawpoint[idx] * (scale / maxdim));
  }
  gtk_panzoom_area_set_offset(this, offset);

  gtk_widget_queue_draw(widget);
  return TRUE;
}

static gboolean gtk_panzoom_area_draw(GtkWidget* widget, cairo_t* cr) {
  GtkPanZoomArea* this = GTK_PANZOOM_AREA(widget);
  GtkPanZoomAreaPrivate* priv =
      gtk_panzoom_area_get_instance_private(GTK_PANZOOM_AREA(widget));
  // draw a white rectangle with black border for the background
  double allocated_width = gtk_widget_get_allocated_width(widget);
  double allocated_height = gtk_widget_get_allocated_height(widget);
  cairo_rectangle(cr, 0, 0, allocated_width, allocated_height);
  cairo_set_source_rgba_gdk(cr, &priv->bg_color);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_stroke_preserve(cr);

  // scale and translate so that we can draw in cartesian coordinates
  cairo_save(cr);
  cairo_clip(cr);
  // make it so that drawing commands in the virtual space map to pixel
  // coordinates
  double max_dim = gtk_panzoom_area_get_max_dim(this);
  double scale = gtk_panzoom_area_get_scale(this);
  double offset[2] = {0, 0};
  gtk_panzoom_area_get_offset(this, offset);

  cairo_scale(cr, max_dim / scale, -max_dim / scale);
  cairo_translate(cr, 0, -scale * allocated_height / max_dim);
  cairo_translate(cr, -offset[0], -offset[1]);

  cairo_set_line_width(cr, 1.0 * scale / max_dim);
  gboolean result = FALSE;
  g_signal_emit(widget, widget_signals[SIGNO_AREA_DRAW], 0, cr, &result);
  cairo_restore(cr);
  return TRUE;
}

static void gtk_panzoom_area_destroy(GtkWidget* widget) {
  GtkPanZoomArea* this = GTK_PANZOOM_AREA(widget);
  GtkPanZoomAreaPrivate* priv = gtk_panzoom_area_get_instance_private(this);

  g_object_unref(G_OBJECT(priv->offset_x));
  priv->offset_x = NULL;
  g_object_unref(G_OBJECT(priv->offset_y));
  priv->offset_y = NULL;
  g_object_unref(G_OBJECT(priv->scale));
  priv->scale = NULL;
  g_object_unref(G_OBJECT(priv->scale_rate));
  priv->scale_rate = NULL;

  GTK_WIDGET_CLASS(gtk_panzoom_area_parent_class)->destroy(widget);
}
