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
#include <fmt/format.h>
#include <gtk/gtk.h>
#include <pHash.h>
#include <tinyxml2.h>

#include "argue/argue.h"
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
  bool use_gtkapplication;
  double threshold;
};

// Context for automatic mode.
struct AutoContext {
  ProgramOpts* opts;       ///< parsed command-line options
  GtkWidget* main_window;  ///< the main GtkWindow toplevel
  std::string outpath;     ///< computed path to the output file where we
                           ///< should write out rendered image
  /// if true, the output_path is a tempfile path and we should unlink it when
  /// we are done
  bool unlink_after_hash;
  /// when the program exists, it will exit() with this code
  int exitcode;
  /// the handler id for the draw-handler that we have registered with the main
  /// window draw signal
  gulong main_window_draw_handler;
};

/// GtkApplication activation callback. Add the main window to the application
/// and show it.
static void activate(GtkApplication* app, gpointer user_data) {
  AutoContext* context = static_cast<AutoContext*>(user_data);
  gtk_application_add_window(app, GTK_WINDOW(context->main_window));
  gtk_widget_show_all(context->main_window);
}

/// GtkPanZoomArea "::draw" signal callback function. Draws a couple of
/// colored shapes.
gboolean sig_draw(GtkWidget* widget, cairo_t* cr) {
  cairo_scale(cr, 0.5, 0.5);
  cairo_translate(cr, 0.5, 0.5);
  cairo_set_line_width(cr, 0.01);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  // Green Triangle
  cairo_move_to(cr, 0.1, 0.1);
  cairo_line_to(cr, 0.9, 0.1);
  cairo_line_to(cr, 0.5, 0.9);
  cairo_line_to(cr, 0.1, 0.1);
  cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.5);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);
  // Red Square
  cairo_move_to(cr, 0.5, 0.3);
  cairo_line_to(cr, 1.2, 0.3);
  cairo_line_to(cr, 1.2, 1.0);
  cairo_line_to(cr, 0.5, 1.0);
  cairo_line_to(cr, 0.5, 0.3);
  cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.5);

  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);
  // Blue Circle
  cairo_move_to(cr, 1.1, 0.1);
  cairo_arc(cr, 0.7, 0.1, 0.4, 0, 2 * M_PI);
  cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.5);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);

  return FALSE;
}

/// Return true if the string `haystack` ends with the string `needle`
bool endswith(const std::string& haystack, const std::string& needle) {
  if (needle.size() > haystack.size()) {
    return false;
  }
  size_t offset = haystack.size() - needle.size();
  return (haystack.substr(offset, needle.size()) == needle);
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

/// Callback to terminate the application when the main window is closed
gboolean main_window_closed(GtkWidget* widget, GdkEvent* event,
                            gpointer user_data) {
  gtk_main_quit();
  return FALSE;
}

/// Callback for automatic mode. This function is called by the Gtk event
/// loop more-or-less immediately after rendering the main window.
gboolean timeout_terminate(gpointer user_data) {
  AutoContext* context = static_cast<AutoContext*>(user_data);
  gtk_widget_destroy(context->main_window);
  return FALSE;
}

/// Callback for automatic mode. This function is called by the Gtk event
/// loop more-or-less immediately after showing the main window.
gboolean timeout_draw(gpointer user_data) {
  AutoContext* context = static_cast<AutoContext*>(user_data);

  cairo_surface_t* cairo_surf = nullptr;
  if (endswith(context->outpath, ".svg")) {
    cairo_surf = cairo_svg_surface_create(context->outpath.c_str(), 800, 600);
  } else if (endswith(context->outpath, ".png")) {
    cairo_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 800, 600);
  } else {
    fmt::print(stderr, "WARNING: Unrecognized file extension: {}\n",
               context->outpath);
    context->exitcode = 1;
    return FALSE;
  }

  if (cairo_surf) {
    cairo_t* cairo_ctx = cairo_create(cairo_surf);
    gtk_widget_draw(context->main_window, cairo_ctx);
    cairo_destroy(cairo_ctx);
  }

  if (endswith(context->outpath, ".png")) {
    cairo_status_t status =
        cairo_surface_write_to_png(cairo_surf, context->outpath.c_str());
    if (status != CAIRO_STATUS_SUCCESS) {
      fmt::print(stderr, "WARNING: Failed to write to: {}\n", context->outpath);
      context->exitcode = 2;
      return FALSE;
    }
  }
  cairo_surface_destroy(cairo_surf);

  if (context->opts->command != "demo") {
    g_timeout_add(1, timeout_terminate, context);
  }

  return FALSE;
}

/// Signal handler for main window when it becomes drawable.
/** This is the earliest known point that we are legally allwed to draw it so we
 *  respond to this signal by adding a timeout to render it with our own
 *  context. */
gboolean main_window_draw(GtkWidget* widget, cairo_t* ignored,
                          gpointer user_data) {
  AutoContext* context = static_cast<AutoContext*>(user_data);
  g_signal_handler_disconnect(G_OBJECT(widget),
                              context->main_window_draw_handler);
  context->main_window_draw_handler = 0;
  g_timeout_add(1, timeout_draw, context);
  return FALSE;
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

  parser->add_argument(
      "-a", "--use-gtk-application", action="store_true",
      dest=&opts->use_gtkapplication,
      help="Use GtkApplication instead of gtk_main().");

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
  parser_opts.name = "gtk_panzoom_demo";
  parser_opts.author = "Josh Bialkowski";
  parser_opts.copyright = "Copyright 2019";

  argue::Parser parser{parser_opts};
  ProgramOpts opts{};
  setup_parser(&parser, &opts);

  gtk_init(&argc, &argv);
  int parse_result = parser.parse_args(argc, argv);
  switch (parse_result) {
    case argue::PARSE_ABORTED:
      exit(0);
    case argue::PARSE_EXCEPTION:
      exit(1);
    case argue::PARSE_FINISHED:
      break;
  }

  GtkWidget* panzoom = gtk_panzoom_area_new();
  GtkBuilder* builder = gtk_builder_new_from_file(opts.glade_filepath.c_str());
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
    deserialize_models(content, builder);
  }

  GObject* main_window_obj = gtk_builder_get_object(builder, "main");
  if (!main_window_obj) {
    fmt::print(stderr, "ERROR: No gtk builder element named {}\n", "main");
    exit(1);
  }

  GObject* panzoom_obj = gtk_builder_get_object(builder, "panzoom");
  if (panzoom_obj) {
    gtk_widget_destroy(panzoom);
    panzoom = GTK_WIDGET(panzoom_obj);
  } else {
    GtkBox* box = GTK_BOX(gtk_builder_get_object(builder, "box"));
    if (!box) {
      fmt::print(stderr, "ERROR: Failed to lookup box");
      exit(1);
    }
    gtk_box_pack_start(box, panzoom, true, true, 0);
    gtk_box_reorder_child(box, panzoom, 0);

    g_object_set(G_OBJECT(panzoom), "offset-x-adjustment",
                 gtk_builder_get_object(builder, "offset_x_adjustment"), NULL);
    g_object_set(G_OBJECT(panzoom), "offset-y-adjustment",
                 gtk_builder_get_object(builder, "offset_y_adjustment"), NULL);
    g_object_set(G_OBJECT(panzoom), "scale-adjustment",
                 gtk_builder_get_object(builder, "scale_adjustment"), NULL);
    g_object_set(G_OBJECT(panzoom), "scale-rate-adjustment",
                 gtk_builder_get_object(builder, "scale_rate_adjustment"),
                 NULL);
  }

  if (opts.draw_with_signal) {
    g_object_connect(G_OBJECT(panzoom), "signal::area-draw",
                     G_CALLBACK(sig_draw), NULL);
  } else {
    g_object_set(G_OBJECT(panzoom), "demo-draw-enabled", TRUE, NULL);
  }

  AutoContext context{};
  context.opts = &opts;
  context.main_window = GTK_WIDGET(main_window_obj);
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
    // g_object_connect(main_window_obj, "signal::draw",
    //                  G_CALLBACK(main_window_draw), &context, NULL);
    context.main_window_draw_handler = g_signal_connect(
        main_window_obj, "draw", G_CALLBACK(main_window_draw), &context);
  }

  if (opts.use_gtkapplication) {
    GtkApplication* app =
        gtk_application_new("tangent.GtkPanZoomDemo", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &context);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    if (status != 0) {
      fmt::print(stderr, "WARNING: GtkApplication exited with {}\n", status);
    }
    g_object_unref(app);
  } else {
    // g_object_connect(main_window_obj, "signal::delete-event",
    //                  G_CALLBACK(main_window_closed), NULL);
    g_signal_connect(main_window_obj, "destroy", G_CALLBACK(main_window_closed),
                     NULL);
    gtk_widget_show_all(GTK_WIDGET(main_window_obj));
    gtk_main();
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
             "WARNING: Rendered window does not match expected percetual "
             "hash to within a threshold of {}\n",
             opts.threshold);
  fmt::print(stderr, "    Note: err={}, pcc={},\n", err, pcc);
  fmt::print(stderr, "    Note: ref-hash=\"{}\"\n", opts.reference_hash);
  fmt::print(stderr, "    Note: act-hash=\"{}\"\n", computed_hash);
  exit(1);
}
