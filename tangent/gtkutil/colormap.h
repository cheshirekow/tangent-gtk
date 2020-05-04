#pragma once
// Copyright 2018 Josh Bialkowski <josh.bialkowski@gmail.com>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <type_traits>

namespace colormap {

template <typename T>
struct Color3 {
  typedef T value_type;
  T r;
  T g;
  T b;
};

typedef Color3<unsigned int> Color3u;
typedef Color3<float> Color3f;

template <class Color>
Color lerp(const Color a, const Color b, double interp) {
  return Color{
      static_cast<typename Color::value_type>(a.r * interp +
                                              b.r * (1.0 - interp)),  //
      static_cast<typename Color::value_type>(a.g * interp +
                                              b.g * (1.0 - interp)),  //
      static_cast<typename Color::value_type>(a.b * interp +
                                              b.b * (1.0 - interp)),
  };
}

struct Range {
  double min;
  double max;
};

template <typename T, typename U>
void convert(const Color3<T> in, Color3<U>* out) {
  if (std::is_integral<T>::value == std::is_integral<U>::value) {
    out->r = static_cast<U>(in.r);
    out->g = static_cast<U>(in.g);
    out->b = static_cast<U>(in.b);
  } else if (std::is_integral<U>::value) {
    out->r = static_cast<U>(in.r * 255.0);
    out->g = static_cast<U>(in.g * 255.0);
    out->b = static_cast<U>(in.b * 255.0);
  } else {
    out->r = static_cast<U>(in.r / 255.0);
    out->g = static_cast<U>(in.g / 255.0);
    out->b = static_cast<U>(in.b / 255.0);
  }
}

template <typename Color>
class ColorMap {
 public:
  template <size_t N>
  explicit ColorMap(const Color (&supports)[N])
      : supports_{supports}, nsupports_{N}, range_{0, N} {}

  ColorMap(const Color* supports, size_t nsupports)
      : supports_{supports},
        nsupports_{nsupports},
        range_{0, static_cast<double>(nsupports)} {}

  ColorMap rescale(double x_min, double x_max) const {
    ColorMap rescaled(supports_, nsupports_);
    rescaled.range = {x_min, x_max};
    return rescaled;
  }

  template <class OtherColor>
  OtherColor as(double x) {
    OtherColor out;
    convert(get(x), &out);
    return out;
  }

  Color get(double x) const {
    x = std::min(x, range_.max);
    double float_index =
        ((x - range_.min) / (range_.max - range_.min)) * nsupports_;

    size_t lower_idx = std::max<size_t>(0, std::floor(float_index));
    size_t upper_idx = std::min<size_t>(nsupports_ - 1, std::ceil(float_index));
    if (lower_idx == upper_idx) {
      return supports_[lower_idx];
    }

    double prev = lower_idx * (range_.max - range_.min) / nsupports_;
    double next = upper_idx * (range_.max - range_.min) / nsupports_;
    double interp = (x - prev) / (next - prev);
    return lerp(supports_[upper_idx], supports_[lower_idx], interp);
  }

  size_t get_nsupports() const {
    return nsupports_;
  }

  Range get_range() const {
    return range_;
  }

  Color operator()(double x) const {
    return get(x);
  }

 private:
  const Color* supports_;
  size_t nsupports_;
  Range range_;
};

typedef ColorMap<Color3u> Map3u;
typedef ColorMap<Color3f> Map3f;

enum ColorNames3u {
  ACCENT,
  BLUES,
  BRBG,
  BUGN,
  BUPU,
  CHROMAJS,
  DARK2,
  GNBU,
  WHGNBU,
  GNPU,
  GREENS,
  GREYS,
  ORANGES,
  ORRD,
  PAIRED,
  PARULA,
  PASTEL1,
  PASTEL2,
  PIYG,
  PRGN,
  PUBUGN,
  PUBU,
  PUOR,
  PURD,
  PURPLES,
  RDBU,
  RDWHBU,
  RDGY,
  RDPU,
  RDYLBU,
  RDYLGN,
  REDS,
  SAND,
  SET1,
  SET2,
  SET3,
  SPECTRAL,
  WHYLRD,
  YLGNBU,
  YLGN,
  YLORBR,
  YLORRD,
  YLRD
};

enum ColorNames3f {
  INFERNO,  //
  JET,
  MAGMA,
  MORELAND,
  PLASMA,
  VIRIDIS
};

ColorMap<Color3u> get_map(ColorNames3u name);

ColorMap<Color3f> get_map(ColorNames3f name);

}  // namespace colormap
