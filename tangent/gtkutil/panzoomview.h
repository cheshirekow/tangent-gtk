#pragma once
// Copyright (C) 2014,2019 Josh Bialkowski (josh.bialkowski@gmail.com)

#include <gtkmm.h>
#include <Eigen/Dense>

namespace Gtk {

/// A drawing area with built in mouse handlers for pan-zoom control
class PanZoomView : public Gtk::DrawingArea {
 private:
  Glib::RefPtr<Gtk::Adjustment> offset_x_;  ///< offset of viewport
  Glib::RefPtr<Gtk::Adjustment> offset_y_;  ///< offset of viewport

  /// Size (in virtual units) of the largest dimension of the widget
  Glib::RefPtr<Gtk::Adjustment> scale_;

  /// When the mouse wheel is turned multiply or divide the scale by this
  /// much
  Glib::RefPtr<Gtk::Adjustment> scale_rate_;

  /// The last mouse position (used during pan)
  Eigen::Vector2d last_pos_;

  /// Which mouse button is used for pan
  int pan_button_;
  guint pan_button_mask_;

 public:
  /// Signal is emitted by the on_draw handler, and sends out the context
  /// with appropriate scaling and translation
  sigc::signal<void, const Cairo::RefPtr<Cairo::Context>&> sig_draw;

  /// motion event with transformed coordinates
  sigc::signal<bool, GdkEventMotion*> sig_motion;

  /// button event with transformed coordinates
  sigc::signal<bool, GdkEventButton*> sig_button;

  PanZoomView();

  void SetOffsetAdjustments(Glib::RefPtr<Gtk::Adjustment> offset_x,
                            Glib::RefPtr<Gtk::Adjustment> offset_y);

  void SetScaleAdjustments(Glib::RefPtr<Gtk::Adjustment> scale,
                           Glib::RefPtr<Gtk::Adjustment> scale_rate);

  void SetOffset(const Eigen::Vector2d& offset);

  // Set the scaling and offset such that the given box entirely visible
  void FitBox(const Eigen::Vector2d& bottom_left,
              const Eigen::Vector2d& top_right);

  Eigen::Vector2d GetOffset();

  void SetScale(double scale);

  double GetScale();
  void SetScaleRate(double scale_rate);

  double GetScaleRate();

  /// return the maximum dimension of the
  double GetMaxDim();

  /// Convert the point (x,y) in GTK coordinates, with the origin at the top
  /// left, to a point in traditional cartesian coordinates, where the origin
  /// is at the bottom left.
  Eigen::Vector2d RawPoint(double x, double y);

  /// Return a point in the virtual cartesian plane by applying the offset
  /// and scaling of the viewport
  Eigen::Vector2d TransformPoint(double x, double y);

  template <typename Event>
  void TransformEvent(Event* event) {
    Eigen::Vector2d transformed_point = TransformPoint(event->x, event->y);
    event->x = transformed_point[0];
    event->y = transformed_point[1];
  }

  // overrides base class handlers
  bool on_motion_notify_event(GdkEventMotion* event) override;
  bool on_button_press_event(GdkEventButton* event) override;
  bool on_scroll_event(GdkEventScroll* event) override;
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& ctx) override;
};

}  // namespace Gtk
