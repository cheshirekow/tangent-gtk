#pragma once
// Copyright 2019 Josh Bialkowski <josh.bialkowski@gmail.com>

#include <map>
#include <string>

#include <gtk/gtk.h>
#include <tinyxml2.h>

#include "tangent/gtkutil/messages.pb.h"
#include "tangent/json/json.h"
#include "tangent/json/type_registry.h"

/// Lookup the builder object corresponding to the given element, and
/// serialize it to the output stream.
void serialize_object(tangent::proto::UIStateEntry* out, GtkBuilder* builder,
                      tinyxml2::XMLElement* elmnt);

/// Serialize all the data models from the gtk GUI to a JSON file.
void serialize_models(tangent::proto::UIState* out, GtkBuilder* builder,
                      tinyxml2::XMLElement* elmnt);

/// Read data model from the protobuf object and update them
void deserialize_object(const tangent::proto::UIStateEntry& msg,
                        GtkBuilder* builder);

/// Read data model values from the protobuf message and update them.
void deserialize_models(const tangent::proto::UIState& msg,
                        GtkBuilder* builder);
