// Copyright (C) 2014,2019 Josh Bialkowski (josh.bialkowski@gmail.com)

#include "tangent/gtkutil/panzoomview.h"

#include <algorithm>

namespace Gtk {

PanZoomView::PanZoomView() {
  add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_MOTION_MASK |
             Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
             Gdk::SCROLL_MASK);
  offset_x_ = Gtk::Adjustment::create(0, -1e9, 1e9);
  offset_y_ = Gtk::Adjustment::create(0, -1e9, 1e9);
  scale_ = Gtk::Adjustment::create(1, 1e-9, 1e9);
  scale_rate_ = Gtk::Adjustment::create(1.05, 0, 10);
  pan_button_ = 3;
  pan_button_mask_ = GDK_BUTTON3_MASK;

  Gdk::EventMask events = Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_MOTION_MASK |
                          Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
                          Gdk::SCROLL_MASK;
  this->add_events(events);
}

void PanZoomView::SetOffsetAdjustments(Glib::RefPtr<Gtk::Adjustment> offset_x,
                                       Glib::RefPtr<Gtk::Adjustment> offset_y) {
  offset_x_ = offset_x;
  offset_y_ = offset_y;
  offset_x_->signal_value_changed().connect(
      sigc::mem_fun(this, &Gtk::DrawingArea::queue_draw));
  offset_y_->signal_value_changed().connect(
      sigc::mem_fun(this, &Gtk::DrawingArea::queue_draw));
}

void PanZoomView::SetScaleAdjustments(
    Glib::RefPtr<Gtk::Adjustment> scale,
    Glib::RefPtr<Gtk::Adjustment> scale_rate) {
  scale_ = scale;
  scale_rate_ = scale_rate;
  scale_->signal_value_changed().connect(
      sigc::mem_fun(this, &Gtk::DrawingArea::queue_draw));
}

void PanZoomView::SetOffset(const Eigen::Vector2d& offset) {
  if (offset_x_) {
    offset_x_->set_value(offset[0]);
  }
  if (offset_y_) {
    offset_y_->set_value(offset[1]);
  }
}

void PanZoomView::FitBox(const Eigen::Vector2d& bottom_left,
                         const Eigen::Vector2d& top_right) {
  SetOffset(bottom_left);
  Eigen::Vector2d dims = top_right - bottom_left;
  SetScale(std::max(dims[0], dims[1]));
}

Eigen::Vector2d PanZoomView::GetOffset() {
  return (offset_x_ && offset_y_)
             ? Eigen::Vector2d(offset_x_->get_value(), offset_y_->get_value())
             : Eigen::Vector2d(0, 0);
}

void PanZoomView::SetScale(double scale) {
  if (scale_) {
    scale_->set_value(scale);
  }
}

double PanZoomView::GetScale() {
  return scale_ ? scale_->get_value() : 1;
}

void PanZoomView::SetScaleRate(double scale_rate) {
  if (scale_rate_) {
    scale_rate_->set_value(scale_rate);
  }
}

double PanZoomView::GetScaleRate() {
  return scale_rate_ ? scale_rate_->get_value() : 1.05;
}

double PanZoomView::GetMaxDim() {
  return std::max(get_allocated_width(), get_allocated_height());
}

Eigen::Vector2d PanZoomView::RawPoint(double x, double y) {
  return Eigen::Vector2d(x, get_allocated_height() - y);
}

Eigen::Vector2d PanZoomView::TransformPoint(double x, double y) {
  return GetOffset() + RawPoint(x, y) * (GetScale() / GetMaxDim());
}

bool PanZoomView::on_motion_notify_event(GdkEventMotion* event) {
  Gtk::DrawingArea::on_motion_notify_event(event);
  GdkEventMotion transformed = *event;
  TransformEvent(&transformed);
  bool handled_by_transformed = sig_motion.emit(&transformed);
  if (handled_by_transformed) {
    queue_draw();
    return true;
  }

  if (event->state & pan_button_mask_) {
    Eigen::Vector2d loc = RawPoint(event->x, event->y);
    Eigen::Vector2d delta = loc - last_pos_;
    Eigen::Vector2d new_offset =
        GetOffset() - (GetScale() / GetMaxDim()) * delta;
    SetOffset(new_offset);
    last_pos_ = loc;
    queue_draw();
    return true;
  }

  return false;
}

bool PanZoomView::on_button_press_event(GdkEventButton* event) {
  Gtk::DrawingArea::on_button_press_event(event);
  GdkEventButton transformed = *event;
  TransformEvent(&transformed);
  bool handled_by_transformed = sig_button.emit(&transformed);

  if (handled_by_transformed) {
    queue_draw();
    return true;
  }

  if (event->button == pan_button_) {
    last_pos_ = RawPoint(event->x, event->y);
    queue_draw();
    return true;
  }
  return false;
}

bool PanZoomView::on_scroll_event(GdkEventScroll* event) {
  // after the change in scale, we want the mouse pointer to be over
  // the same location in the scaled view
  Eigen::Vector2d raw_point = RawPoint(event->x, event->y);
  Eigen::Vector2d center_point = TransformPoint(event->x, event->y);

  if (event->direction == GDK_SCROLL_UP) {
    if (scale_) {
      scale_->set_value(GetScale() * GetScaleRate());
    }
  } else if (event->direction == GDK_SCROLL_DOWN) {
    if (scale_) {
      scale_->set_value(GetScale() / GetScaleRate());
    }
  } else {
    Gtk::DrawingArea::on_scroll_event(event);
    return true;
  }

  // center_point = offset + raw_point * scale / max_dim
  // offset = center_point - raw_point * scale / max_dim
  SetOffset(center_point - raw_point * (GetScale() / GetMaxDim()));
  queue_draw();
  return true;
}

bool PanZoomView::on_draw(const Cairo::RefPtr<Cairo::Context>& ctx) {
  // draw a white rectangle for the background
  ctx->rectangle(0, 0, get_allocated_width(), get_allocated_height());
  ctx->set_source_rgb(1, 1, 1);
  ctx->fill_preserve();
  ctx->set_source_rgb(0, 0, 0);
  ctx->stroke();

  // scale and translate so that we can draw in cartesian coordinates
  ctx->save();
  // make it so that drawing commands in the virtual space map to pixel
  // coordinates
  ctx->scale(GetMaxDim() / GetScale(), -GetMaxDim() / GetScale());
  ctx->translate(0, -GetScale() * get_allocated_height() / GetMaxDim());
  ctx->translate(-GetOffset()[0], -GetOffset()[1]);

  ctx->set_line_width(0.001);

  sig_draw.emit(ctx);
  ctx->restore();

  return true;
}

}  // namespace Gtk
