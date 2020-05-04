#pragma once
// Copyright (C) 2012,2019 Josh Bialkowski (josh.bialkowski@gmail.com)
#include "tangent/gtkutil/eigencairo.h"

namespace eigencairo {

template <typename Derived>
void translate(cairo_t* cr, const Eigen::MatrixBase<Derived>& v) {
  cairo_translate(cr, static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived>
void scale(cairo_t* cr, const Eigen::MatrixBase<Derived>& v) {
  cairo_scale(cr, static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived>
void user_to_device(cairo_t* cr, Eigen::MatrixBase<Derived>* v) {
  double x = (*v)[0];
  double y = (*v)[1];
  cairo_user_to_device(cr, &x, &y);
  (*v)[0] = x;
  (*v)[1] = y;
}

template <typename Derived>
void move_to(cairo_t* cr, const Eigen::MatrixBase<Derived>& v) {
  cairo_move_to(cr, static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived>
void line_to(cairo_t* cr, const Eigen::MatrixBase<Derived>& v) {
  cairo_line_to(cr, static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived1, typename Derived2, typename Derived3>
void curve_to(cairo_t* cr, const Eigen::MatrixBase<Derived2>& c1,
              const Eigen::MatrixBase<Derived3>& c2,
              const Eigen::MatrixBase<Derived1>& x) {
  cairo_curve_to(cr, static_cast<double>(c1[0]), static_cast<double>(c1[1]),
                 static_cast<double>(c2[0]), static_cast<double>(c2[1]),
                 static_cast<double>(x[0]), static_cast<double>(x[1]));
}

template <typename Derived>
void arc(cairo_t* cr, const Eigen::MatrixBase<Derived>& c, double radius,
         double angle1, double angle2) {
  cairo_arc(cr, static_cast<double>(c[0]), static_cast<double>(c[1]), radius,
            angle1, angle2);
}

template <typename Derived>
void arc_negative(cairo_t* cr, const Eigen::MatrixBase<Derived>& c,
                  double radius, double angle1, double angle2) {
  cairo_arc_negative(cr, static_cast<double>(c[0]), static_cast<double>(c[1]),
                     radius, angle1, angle2);
}

template <typename Derived>
void rel_move_to(cairo_t* cr, const Eigen::MatrixBase<Derived>& v) {
  cairo_rel_move_to(cr, static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived>
void rel_line_to(cairo_t* cr, const Eigen::MatrixBase<Derived>& v) {
  cairo_rel_line_to(cr, static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived1, typename Derived2, typename Derived3>
void rel_curve_to(cairo_t* cr, const Eigen::MatrixBase<Derived1>& dx,
                  const Eigen::MatrixBase<Derived2>& dc1,
                  const Eigen::MatrixBase<Derived3>& dc2) {
  cairo_rel_curve_to(cr, static_cast<double>(dx[0]), static_cast<double>(dx[1]),
                     static_cast<double>(dc1[0]), static_cast<double>(dc1[1]),
                     static_cast<double>(dc2[0]), static_cast<double>(dc2[1]));
}

template <typename Derived1, typename Derived2>
void rectangle(cairo_t* cr, const Eigen::MatrixBase<Derived1>& x,
               const Eigen::MatrixBase<Derived2>& s) {
  cairo_rectangle(cr, static_cast<double>(x[0]), static_cast<double>(x[1]),
                  static_cast<double>(s[0]), static_cast<double>(s[1]));
}

template <typename Derived>
inline void circle(cairo_t* cr, const Eigen::MatrixBase<Derived>& c, double r) {
  cairo_move_to(cr, static_cast<double>(c[0]) + r, static_cast<double>(c[1]));
  arc(cr, c, r, 0, 2 * M_PI);
}

}  // namespace eigencairo
