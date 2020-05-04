// Copyright (C) 2014,2019 Josh Bialkowski (josh.bialkowski@gmail.com)
#include <cstdio>
#include <fstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fmt/format.h>
#include <gtk/gtk.h>
#include <tinyxml2.h>

#include "argue/argue.h"
#include "tangent/gtkutil/serializemodels.h"

struct ProgramOpts {
  std::string input_filepath;
  std::string output_filepath;
  std::string glade_filepath;
  std::string main_window;
  bool test;
};

static void activate(GtkApplication* app, gpointer user_data) {
  gtk_application_add_window(app, GTK_WINDOW(user_data));
  gtk_widget_show_all(GTK_WIDGET(user_data));
}

static gboolean quit_test(gpointer main_window_obj) {
  gtk_widget_destroy(GTK_WIDGET(main_window_obj));
  return FALSE;
}

int main(int argc, char** argv) {
  argue::Parser::Metadata parser_opts{};
  parser_opts.add_help = true;
  parser_opts.name = "gtk_serialize_demo";
  parser_opts.author = "Josh Bialkowski";
  parser_opts.copyright = "Copyright 2019";

  argue::Parser parser{parser_opts};
  ProgramOpts opts{};

  {
    argue::KWargs<std::string> kwargs{};
    kwargs.help = "Path to the JSON file from which to load data";
    parser.add_argument("-i", "--input", &opts.input_filepath, kwargs);
  }
  {
    argue::KWargs<std::string> kwargs{};
    kwargs.help = "Path to the JSON file where to save data";
    parser.add_argument("-o", "--output", &opts.output_filepath, kwargs);
  }
  {
    argue::KWargs<std::string> kwargs{};
    kwargs.help = "Path to the gladefile to load";
    parser.add_argument("glade_filepath", &opts.glade_filepath, kwargs);
  }
  {
    argue::KWargs<std::string> kwargs{};
    kwargs.help = "Name of the main window to run the application";
    kwargs.default_ = "main";
    parser.add_argument("-m", "--main", &opts.main_window, kwargs);
  }
  {
    argue::KWargs<bool> kwargs{};
    kwargs.action = "store_true";
    kwargs.help = "Execute in test mode. Quit the application immediately";
    parser.add_argument("-t", "--test", &opts.test, kwargs);
  }

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

  GtkBuilder* builder = gtk_builder_new_from_file(opts.glade_filepath.c_str());
  if (!builder) {
    fmt::print(stderr, "ERROR: Failed to load gladefile {}\n",
               opts.glade_filepath);
    exit(1);
  }

  tinyxml2::XMLDocument doc{};
  if (doc.LoadFile(opts.glade_filepath.c_str()) != tinyxml2::XML_SUCCESS) {
    fmt::print(stderr, "ERROR: Failed to parse gladefile xml for {}\n",
               opts.glade_filepath);
    exit(1);
  }

  tinyxml2::XMLElement* root_elmnt = doc.RootElement();
  if (!root_elmnt) {
    fmt::print(stderr, "ERROR: No root element in {}\n", opts.glade_filepath);
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

  GObject* main_window_obj =
      gtk_builder_get_object(builder, opts.main_window.c_str());
  if (!main_window_obj) {
    fmt::print(stderr, "ERROR: No gtk builder element named {}\n",
               opts.main_window);
    exit(1);
  }

  GtkApplication* app =
      gtk_application_new("tangent.GtkSerialDemo", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), main_window_obj);

  if (opts.test) {
    g_timeout_add(50, quit_test, main_window_obj);
  }
  int status = g_application_run(G_APPLICATION(app), 0, NULL);
  if (status != 0) {
    fmt::print(stderr, "WARNING: GtkApplication exited with {}\n", status);
  }
  g_object_unref(app);

  if (!opts.output_filepath.empty()) {
    std::ostream* outfile;
    std::ofstream outfile_stream;
    if (opts.output_filepath == "-") {
      outfile = &std::cout;
    } else {
      outfile_stream.open(opts.output_filepath);
      outfile = &outfile_stream;
    }
    fmt::print(*outfile, "{{\n");
    serialize_models(outfile, builder, root_elmnt);
    fmt::print(*outfile, "}}\n");
  }
}
