#pragma once
// Copyright 2019 Josh Bialkowski <josh.bialkowski@gmail.com>

#include <map>
#include <string>

#include <gtk/gtk.h>
#include <tinyxml2.h>

#include "tangent/json/json.h"
#include "tangent/json/type_registry.h"

/// Lookup the builder object corresponding to the given element, and
/// serialize it to the output stream.
/* Note that this function will write a trailing comma after the key/value
 * pair. It is expected that at least one more key will be written to the
 * output stream within the current JSON object. */
void serialize_object(std::ostream* out, GtkBuilder* builder,
                      tinyxml2::XMLElement* elmnt);

/// Serialize all the data models from the gtk GUI to a JSON file.
/*  Note that this function does not write the initial or closing object
 *  brackets ({}). This allows it to be combined with additional JSON output
 *  streams. */
void serialize_models(std::ostream* out, GtkBuilder* builder,
                      tinyxml2::XMLElement* elmnt);

/// Read data model values from the JSON file and update them
int deserialize_models(const std::string& content, GtkBuilder* builder);

/// Read data model values from the JSON file and update them. Values are
/// expected nested beneath the "builder_models" key.
int deserialize_models(json::LexerParser* parser, GtkBuilder* builder);

/// Read data models from the JSON object and update them
int deserialize_object(json::LexerParser* parser, GtkBuilder* builder);

struct GtkBuilderPair {
  GtkBuilder* builder;
  tinyxml2::XMLDocument* document;

  static int parsefield(const json::stream::Registry& registry,
                        const re2::StringPiece& key, json::LexerParser* stream,
                        GtkBuilderPair* out);

  static int dumpfields(const GtkBuilderPair& out,
                        json::stream::Dumper* dumper);
};
