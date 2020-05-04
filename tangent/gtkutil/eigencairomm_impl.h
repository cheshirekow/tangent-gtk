#pragma once
// Copyright (C) 2012,2019 Josh Bialkowski (josh.bialkowski@gmail.com)
#include "tangent/gtkutil/eigencairomm.h"

namespace eigencairomm {

template <typename Derived>
void translate(const Cairo::RefPtr<Cairo::Context>& ctx,
               const Eigen::MatrixBase<Derived>& v) {
  ctx->translate(static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived>
void scale(const Cairo::RefPtr<Cairo::Context>& ctx,
           const Eigen::MatrixBase<Derived>& v) {
  ctx->scale(static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived>
void user_to_device(const Cairo::RefPtr<Cairo::Context>& ctx,
                    Eigen::MatrixBase<Derived>* v) {
  double x = (*v)[0];
  double y = (*v)[1];
  ctx->user_to_device(x, y);
  (*v)[0] = x;
  (*v)[1] = y;
}

void user_to_device(const Cairo::RefPtr<Cairo::Context>& ctx,
                    Eigen::Matrix<double, 2, 1>* v) {
  ctx->user_to_device((*v)[0], (*v)[1]);
}

template <typename Derived>
void move_to(const Cairo::RefPtr<Cairo::Context>& ctx,
             const Eigen::MatrixBase<Derived>& v) {
  ctx->move_to(static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived>
void line_to(const Cairo::RefPtr<Cairo::Context>& ctx,
             const Eigen::MatrixBase<Derived>& v) {
  ctx->line_to(static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived1, typename Derived2, typename Derived3>
void curve_to(const Cairo::RefPtr<Cairo::Context>& ctx,
              const Eigen::MatrixBase<Derived1>& x,
              const Eigen::MatrixBase<Derived2>& c1,
              const Eigen::MatrixBase<Derived3>& c2) {
  ctx->curve_to(static_cast<double>(x[0]), static_cast<double>(x[1]),
                static_cast<double>(c1[0]), static_cast<double>(c1[1]),
                static_cast<double>(c2[0]), static_cast<double>(c2[1]));
}

template <typename Derived>
void arc(const Cairo::RefPtr<Cairo::Context>& ctx,
         const Eigen::MatrixBase<Derived>& c, double radius, double angle1,
         double angle2) {
  ctx->arc(static_cast<double>(c[0]), static_cast<double>(c[1]), radius, angle1,
           angle2);
}

template <typename Derived>
void arc_negative(const Cairo::RefPtr<Cairo::Context>& ctx,
                  const Eigen::MatrixBase<Derived>& c, double radius,
                  double angle1, double angle2) {
  ctx->arc_negative(static_cast<double>(c[0]), static_cast<double>(c[1]),
                    radius, angle1, angle2);
}

template <typename Derived>
void rel_move_to(const Cairo::RefPtr<Cairo::Context>& ctx,
                 const Eigen::MatrixBase<Derived>& v) {
  ctx->rel_move_to(static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived>
void rel_line_to(const Cairo::RefPtr<Cairo::Context>& ctx,
                 const Eigen::MatrixBase<Derived>& v) {
  ctx->rel_line_to(static_cast<double>(v[0]), static_cast<double>(v[1]));
}

template <typename Derived1, typename Derived2, typename Derived3>
void rel_curve_to(const Cairo::RefPtr<Cairo::Context>& ctx,
                  const Eigen::MatrixBase<Derived1>& dx,
                  const Eigen::MatrixBase<Derived2>& dc1,
                  const Eigen::MatrixBase<Derived3>& dc2) {
  ctx->rel_curve_to(static_cast<double>(dx[0]), static_cast<double>(dx[1]),
                    static_cast<double>(dc1[0]), static_cast<double>(dc1[1]),
                    static_cast<double>(dc2[0]), static_cast<double>(dc2[1]));
}

template <typename Derived1, typename Derived2>
void rectangle(const Cairo::RefPtr<Cairo::Context>& ctx,
               const Eigen::MatrixBase<Derived1>& x,
               const Eigen::MatrixBase<Derived2>& s) {
  ctx->rectangle(static_cast<double>(x[0]), static_cast<double>(x[1]),
                 static_cast<double>(s[0]), static_cast<double>(s[1]));
}

template <typename Derived>
inline void circle(const Cairo::RefPtr<Cairo::Context>& ctx,
                   const Eigen::MatrixBase<Derived>& c, double r) {
  ctx->move_to(static_cast<double>(c[0]) + r, static_cast<double>(c[1]));
  ctx->arc(c, r, 0, 2 * M_PI);
}

}  // namespace eigencairomm
