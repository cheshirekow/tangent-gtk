// Copyright (C) 2014,2019 Josh Bialkowski (josh.bialkowski@gmail.com)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include <cairo/cairo-svg.h>
#include <cairo/cairo.h>
#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <fmt/format.h>
#include <glibmm/main.h>
#include <gtk/gtk.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/builder.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <pHash.h>
#include <tinyxml2.h>

#include "argue/argue.h"
#include "tangent/gtkutil/mm/panzoomarea.h"
#include "tangent/gtkutil/mm/wrap_init.h"
#include "tangent/gtkutil/panzoomarea.h"
#include "tangent/gtkutil/serializemodels.h"

/// Parsed command line options
struct ProgramOpts {
  std::string glade_filepath;
  std::string outfile_path;
  std::string input_filepath;
  std::string reference_hash;
  std::string command;
  bool draw_with_signal;
  double threshold;
};

/// Return true if the string `haystack` ends with the string `needle`
bool endswith(const std::string& haystack, const std::string& needle) {
  if (needle.size() > haystack.size()) {
    return false;
  }
  size_t offset = haystack.size() - needle.size();
  return (haystack.substr(offset, needle.size()) == needle);
}

// Context for automatic mode.
struct AutoContext {
  ProgramOpts* opts;     ///< parsed command-line options
  Gtk::Window* mainwin;  ///< the main GtkWindow toplevel
  std::string outpath;   ///< computed path to the output file where we
                         ///< should write out rendered image
  /// if true, the output_path is a tempfile path and we should unlink it when
  /// we are done
  bool unlink_after_hash;

  /// when the program exists, it will exit() with this code
  int exitcode;

  /// the handler id for the draw-handler that we have registered with the main
  /// window draw signal
  sigc::connection mainwin_draw_connection;

  bool mainwin_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    mainwin_draw_connection.disconnect();
    Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &AutoContext::timeout_draw), 1);
    return false;
  }

  bool timeout_draw() {
    Cairo::RefPtr<Cairo::Surface> cairo_surf;
    if (endswith(outpath, ".svg")) {
      cairo_surf = Cairo::SvgSurface::create(outpath, 800, 600);
    } else if (endswith(outpath, ".png")) {
      cairo_surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, 800, 600);
    } else {
      fmt::print(stderr, "WARNING: Unrecognized file extension: {}\n", outpath);
      exitcode = 1;
      return false;
    }

    if (cairo_surf) {
      Cairo::RefPtr<Cairo::Context> cairo_ctx =
          Cairo::Context::create(cairo_surf);
      // NOTE(josh): draw() is protected? hm...
      // mainwin->draw(cairo_ctx);
      gtk_widget_draw(GTK_WIDGET(mainwin->gobj()), cairo_ctx->cobj());
    }

    if (endswith(outpath, ".png")) {
      try {
        cairo_surf->write_to_png(outpath);
      } catch (const std::exception& ex) {
        exitcode = 2;
        return false;
      }
    }

    if (opts->command != "demo") {
      Glib::signal_timeout().connect(
          sigc::mem_fun(*this, &AutoContext::timeout_terminate), 1);
    }

    return false;
  }

  bool timeout_terminate() {
    mainwin->close();
  }
};

/// GtkPanZoomArea "::draw" signal callback function. Draws a couple of
/// colored shapes.
bool sig_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
  cr->scale(0.5, 0.5);
  cr->translate(0.5, 0.5);
  cr->set_line_width(0.01);
  cr->set_line_cap(Cairo::LINE_CAP_ROUND);
  cr->set_line_join(Cairo::LINE_JOIN_ROUND);
  // Green Triangle
  cr->move_to(0.1, 0.1);
  cr->line_to(0.9, 0.1);
  cr->line_to(0.5, 0.9);
  cr->line_to(0.1, 0.1);
  cr->set_source_rgba(0.0, 0.0, 1.0, 0.5);
  cr->fill_preserve();
  cr->set_source_rgb(0.0, 0.0, 0.0);
  cr->stroke();
  // Red Square
  cr->move_to(0.5, 0.3);
  cr->line_to(1.2, 0.3);
  cr->line_to(1.2, 1.0);
  cr->line_to(0.5, 1.0);
  cr->line_to(0.5, 0.3);
  cr->set_source_rgba(0.0, 1.0, 0.0, 0.5);

  cr->fill_preserve();
  cr->set_source_rgb(0.0, 0.0, 0.0);
  cr->stroke();
  // Blue Circle
  cr->move_to(1.1, 0.1);
  cr->arc(0.7, 0.1, 0.4, 0, 2 * M_PI);
  cr->set_source_rgba(1.0, 0.0, 0.0, 0.5);
  cr->fill_preserve();
  cr->set_source_rgb(0.0, 0.0, 0.0);
  cr->stroke();

  return false;
}

/// Decode a hexadecimal string into a pHash digest structure. Note that this
/// function will allocate digest->coeffs and the caller is responsible to
/// `free()` it.
void decode_hash(ph_digest* digest, const std::string hex) {
  digest->size = hex.length() / 2;
  digest->coeffs = static_cast<uint8_t*>(malloc(digest->size));
  for (size_t idx = 0; idx < hex.length(); idx += 2) {
    std::string byte = hex.substr(idx, 2);
    digest->coeffs[idx / 2] = (uint8_t)std::stoi(byte.c_str(), nullptr, 16);
  }
}

/// Encode a phash digest structure into a hexadecimal string.
std::string encode_hash(const ph_digest& digest) {
  std::string hex;
  hex.reserve(digest.size * 2);
  for (int idx = 0; idx < digest.size; idx++) {
    hex += fmt::format("{:02x}", static_cast<int>(digest.coeffs[idx]));
  }
  return hex;
}

std::string get_tempdir() {
  std::vector<std::string> tryenv = {"TMPDIR", "TMP", "TEMP", "TEMPDIR"};
  for (const std::string& trykey : tryenv) {
    char* found = getenv(trykey.c_str());
    if (found) {
      return std::string(found);
    }
  }
  return "/tmp";
}

/// Configure the command line parser
void setup_parser(argue::Parser* parser, ProgramOpts* opts) {
  using argue::keywords::action;
  using argue::keywords::default_;
  using argue::keywords::dest;
  using argue::keywords::help;
  using argue::keywords::nargs;

  // clang-format off
  parser->add_argument(
      "-u", "--ui", dest=&opts->glade_filepath, default_="./panzoom.ui",
      nargs="?", help="Path to the user-interface specification file to load");

  parser->add_argument(
      "-i", "--input", dest=&opts->input_filepath,
      help="Path to JSON input file specifying initial state of adjustments");

  parser->add_argument(
      "-s", "--draw-signal", action="store_true", dest=&opts->draw_with_signal,
      help="Draw using the callback handler in the demo. Default is to use the "
            "default handler embedded in the widget.");

  auto subparsers =
      parser->add_subparsers("command", &opts->command, {.help = "Subcommand"});

  subparsers->add_parser(
      "demo", {.help = "Draw the gui and allow the user to interact with it"});
  auto render_parser = subparsers->add_parser(
      "render", {.help = "Render the main window to an image file"});

  render_parser->add_argument(
    "outfile", dest=&opts->outfile_path,
    help="Path to the image file to write. Valid extensions are .png or "
         ".svg.");

  auto hash_parser = subparsers->add_parser(
      "hash", {.help = "Render the main window to an image file and compute "
                       "the perceptual hash of it ."});
  auto test_parser = subparsers->add_parser(
      "test", {.help = "Render the main window to an image file, compute the "
                       "perceptual hash of it, and compare against a known "
                       "hash. Exit with exit code '0' if the perceptual "
                       "difference is small, and '1' if it is large."});

  for (auto sub : {hash_parser, test_parser}) {
    sub->add_argument(
      "-o", "--outfile", dest=&opts->outfile_path,
      help="Path to the image file to write. Valid extensions are .png or "
           ".svg. Default will be a temporary file.");
  }

  test_parser->add_argument(
      "--threshold", dest=&opts->threshold, default_=0.9,
      help="Digest difference threshold allowed for test mode");

  test_parser->add_argument(
      "reference_hash", dest=&opts->reference_hash,
      help="Hexadecimal hash string to compare the visual rendering against");

  // clang-format on
}

int main(int argc, char** argv) {
  argue::Parser::Metadata parser_opts{};
  parser_opts.add_help = true;
  parser_opts.name = "gtk-panzoom-demo";
  parser_opts.author = "Josh Bialkowski";
  parser_opts.copyright = "Copyright 2019";

  gtk_init(&argc, &argv);
  Gtk::Main::init_gtkmm_internals();
  Gtk::wrap_init();

  argue::Parser parser{parser_opts};
  ProgramOpts opts{};
  setup_parser(&parser, &opts);
  int parse_result = parser.parse_args(argc, argv);
  switch (parse_result) {
    case argue::PARSE_ABORTED:
      exit(0);
    case argue::PARSE_EXCEPTION:
      exit(1);
    case argue::PARSE_FINISHED:
      break;
  }

  Glib::RefPtr<Gtk::Builder> builder =
      Gtk::Builder::create_from_file(opts.glade_filepath);
  if (!builder) {
    fmt::print(stderr, "ERROR: Failed to load gladefile {}\n",
               opts.glade_filepath);
    exit(1);
  }

  if (!opts.input_filepath.empty()) {
    std::istream* infile = nullptr;
    std::ifstream infile_stream;
    if (opts.input_filepath == "-") {
      infile = &std::cin;
    } else {
      struct stat statbuf {};
      int err = stat(opts.input_filepath.c_str(), &statbuf);
      if (err) {
        fmt::print(stderr, "ERROR: Can't status iput file {}\n",
                   opts.input_filepath);
        exit(1);
      }
      infile_stream.open(opts.input_filepath);
      infile = &infile_stream;
    }

    std::string content;
    content.reserve(1024 * 1024);
    content.assign((std::istreambuf_iterator<char>(*infile)),
                   std::istreambuf_iterator<char>());
    deserialize_models(content, builder->gobj());
  }

  Gtk::Window* mainwin{nullptr};
  builder->get_widget("main", mainwin);
  if (!mainwin) {
    fmt::print(stderr, "ERROR: No gtk builder element named {}\n", "main");
    exit(1);
  }

  Gtk::PanZoomArea* panzoom{nullptr};
  builder->get_widget("panzoom", panzoom);
  if (!panzoom) {
    panzoom = new Gtk::PanZoomArea{};
    Gtk::Box* box{nullptr};
    builder->get_widget("box", box);
    if (!box) {
      fmt::print(stderr, "ERROR: Failed to lookup box");
      exit(1);
    }
    box->pack_start(*panzoom, true, true, 0);
    box->reorder_child(*panzoom, 0);

    panzoom->set_property("offset-x-adjustment",
                          Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(
                              builder->get_object("offset_x_adjustment")));

    panzoom->set_property("offset-y-adjustment",
                          Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(
                              builder->get_object("offset_y_adjustment")));

    panzoom->set_property("scale-adjustment",
                          Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(
                              builder->get_object("scale_adjustement")));

    panzoom->set_property("scale-rate-adjustment",
                          Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(
                              builder->get_object("scale_rate_adjustement")));
  }

  if (opts.draw_with_signal) {
    panzoom->signal_area_draw().connect(&sig_draw);
  } else {
    panzoom->set_property("demo-draw-enabled", true);
  }

  AutoContext context{};
  context.opts = &opts;
  context.mainwin = mainwin;
  context.exitcode = 0;

  if (opts.command != "demo") {
    context.outpath = opts.outfile_path;
    if (context.outpath.empty()) {
      std::string tempdir = get_tempdir();
      context.outpath = fmt::format("{}/panzoomtest-XXXXXX.png", tempdir);
      context.unlink_after_hash = true;
      int outfd = mkstemps(&context.outpath[0], 4);
      if (outfd == -1) {
        fmt::print(stderr,
                   "ERROR: Failed to create temporary file to write: {}\n",
                   strerror(errno));
        exit(1);
      }
      close(outfd);
    }

    context.mainwin_draw_connection = mainwin->signal_draw().connect(
        sigc::mem_fun(context, &AutoContext::mainwin_draw));
  }

  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create();
  mainwin->show_all();
  int status = app->run(*mainwin);
  if (status != 0) {
    fmt::print(stderr, "WARNING: GtkApplication exited with {}\n", status);
  }

  if (context.exitcode) {
    exit(context.exitcode);
  }

  // If we are in demo mode or render mode, then as soon as we are rendered we
  // are done.
  if (opts.command == "demo" || opts.command == "render") {
    exit(0);
  }

  ph_digest compute_digest{};
  ph_image_digest(context.outpath.c_str(), 3.5, 1.0, compute_digest);
  std::string computed_hash = encode_hash(compute_digest);
  if (context.unlink_after_hash) {
    unlink(context.outpath.c_str());
  }

  if (opts.command == "hash") {
    fmt::print(stdout, "{}\n", computed_hash);
    free(compute_digest.coeffs);
    exit(0);
  }

  if (opts.command != "test") {
    fmt::print(stderr, "WARNING: unrecognized command {}\n", opts.command);
    free(compute_digest.coeffs);
    exit(1);
  }

  ph_digest reference_digest{};
  decode_hash(&reference_digest, opts.reference_hash);
  double pcc = 0.0;
  int err = ph_crosscorr(reference_digest, compute_digest, pcc, opts.threshold);
  free(compute_digest.coeffs);
  free(reference_digest.coeffs);
  if (err > 0 && pcc > opts.threshold) {
    exit(0);
  }

  fmt::print(stderr,
             "WARNING: Rendered window does not match expected perceptual "
             "hash to within a threshold of {}\n",
             opts.threshold);
  fmt::print(stderr, "    Note: err={}, pcc={},\n", err, pcc);
  fmt::print(stderr, "    Note: ref-hash=\"{}\"\n", opts.reference_hash);
  fmt::print(stderr, "    Note: act-hash=\"{}\"\n", computed_hash);
  exit(1);
}
