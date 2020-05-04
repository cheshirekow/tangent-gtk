#include <iostream>

#include "tangent/gtkutil/panzoomarea.h"
#include "tangent/gtkutil/protobuf_serialize.h"
#include "tangent/util/hash.h"

void serialize_object(tangent::proto::UIStateEntry* out, GtkBuilder* builder,
                      tinyxml2::XMLElement* elem) {
  const char* class_str = elem->Attribute("class");
  const char* name_str = elem->Attribute("id");

  if (!name_str) {
    return;
  }
  GObject* obj = gtk_builder_get_object(builder, name_str);
  if (!obj) {
    return;
  }
  out->set_name(name_str);
  out->set_class_(class_str);

  switch (tangent::runtime_hash(class_str)) {
    case tangent::hash("GtkAdjustment"): {
      double value = gtk_adjustment_get_value(GTK_ADJUSTMENT(obj));
      out->mutable_value()->set_double_value(value);
      break;
    }
    case tangent::hash("GtkCheckButton"):
    case tangent::hash("GtkToggleButton"):
    case tangent::hash("GtkRadioButton"): {
      bool value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj));
      out->mutable_value()->set_bool_value(value);
      break;
    }
    case tangent::hash("GtkColorButton"): {
      GdkRGBA color;
      gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(obj), &color);
      auto* rgba_out = out->mutable_value()->mutable_rgba_value();
      rgba_out->set_red(color.red);
      rgba_out->set_green(color.green);
      rgba_out->set_blue(color.blue);
      rgba_out->set_alpha(color.alpha);
      break;
    }
    case tangent::hash("GtkComboBoxText"): {
      gchar* value =
          gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(obj));
      if (value) {
        out->mutable_value()->set_string_value(value);
        g_free(value);
      }
      break;
    }
    case tangent::hash("GtkEntryBuffer"): {
      const gchar* value = gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(obj));
      out->mutable_value()->set_string_value(value);
      break;
    }
    case tangent::hash("GtkTextBuffer"): {
      GtkTextIter start;
      GtkTextIter end;
      gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(obj), &start);
      gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(obj), &end);
      gchar* value =
          gtk_text_buffer_get_text(GTK_TEXT_BUFFER(obj), &start, &end,
                                   /*include_hidden_chars=*/false);
      out->mutable_value()->set_string_value(value);
      g_free(value);
      break;
    }

    case tangent::hash("GtkPanZoomArea"): {
      double offset[2] = {0, 0};
      gtk_panzoom_area_get_offset(GTK_PANZOOM_AREA(obj), offset);
      auto* panzoom_out = out->mutable_value()->mutable_panzoom_value();
      panzoom_out->set_offset_x(offset[0]);
      panzoom_out->set_offset_y(offset[1]);
      panzoom_out->set_scale(gtk_panzoom_area_get_scale(GTK_PANZOOM_AREA(obj)));
      panzoom_out->set_scale_rate(
          gtk_panzoom_area_get_scale_rate(GTK_PANZOOM_AREA(obj)));
      break;
    }

    default: {
      std::cerr << "No known dumper for " << class_str << "\n";
    }
      // TODO(josh): GtkPaned so we can preserve pane locations
  }
}

static void serialize_recurse(tangent::proto::UIState* out, GtkBuilder* builder,
                              tinyxml2::XMLElement* elmnt) {
  tinyxml2::XMLElement* obj;
  tinyxml2::XMLElement* child;

  // iterate over all objects
  obj = elmnt->FirstChildElement("object");
  while (obj) {
    auto* entry = out->add_entry();
    serialize_object(entry, builder, obj);
    serialize_recurse(out, builder, obj);
    obj = obj->NextSiblingElement("object");
  }

  // iterate over all children and recurse into
  child = elmnt->FirstChildElement("child");
  while (child) {
    serialize_recurse(out, builder, child);
    child = child->NextSiblingElement("child");
  }
}

void serialize_models(tangent::proto::UIState* out, GtkBuilder* builder,
                      tinyxml2::XMLElement* elmnt) {
  serialize_recurse(out, builder, elmnt);
}

void deserialize_object(const tangent::proto::UIStateEntry& msg,
                        GtkBuilder* builder) {
  if (msg.name().empty()) {
    return;
  }
  GObject* obj = gtk_builder_get_object(builder, msg.name().c_str());
  if (!obj) {
    std::cerr << "WARNING: unrecognized UI key " << msg.name() << "\n";
    return;
  }

  switch (msg.value().variant_value_case()) {
    case tangent::proto::ModelVariant::kDoubleValue: {
      if (GTK_IS_ADJUSTMENT(obj)) {
        gtk_adjustment_set_value(GTK_ADJUSTMENT(obj),
                                 msg.value().double_value());
      } else {
        std::cerr << "WARNING: expected an adjustment for " << msg.name()
                  << " but got something else (" << G_OBJECT_TYPE(obj) << ")";
      }
      return;
    }

    case tangent::proto::ModelVariant::kBoolValue: {
      if (GTK_IS_CHECK_BUTTON(obj) || GTK_IS_TOGGLE_BUTTON(obj) ||
          GTK_IS_RADIO_BUTTON(obj)) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(obj),
                                     msg.value().bool_value());

      } else {
        std::cerr << "WARNING: expected a toggle button for " << msg.name()
                  << " but got something else (" << G_OBJECT_TYPE(obj) << ")";
      }
      return;
    }

    case tangent::proto::ModelVariant::kStringValue: {
      std::string value = msg.value().string_value();
      if (GTK_IS_COMBO_BOX_TEXT(obj)) {
        std::cerr << "WARNING: deserialize combobox is not yet implemented ";
      } else if (GTK_IS_ENTRY_BUFFER(obj)) {
        gtk_entry_buffer_set_text(GTK_ENTRY_BUFFER(obj), &value[0],
                                  value.size());
      } else if (GTK_IS_TEXT_BUFFER(obj)) {
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(obj), &value[0], value.size());
      }
      return;
    }

    case tangent::proto::ModelVariant::kRgbaValue: {
      if (!GTK_IS_COLOR_BUTTON(obj)) {
        std::cerr << "WARNING: expected color button for " << msg.name()
                  << " but got something else (" << G_OBJECT_TYPE(obj) << ")";
        return;
      }
      const auto& color_msg = msg.value().rgba_value();
      GdkRGBA color;
      color.red = color_msg.red();
      color.green = color_msg.green();
      color.blue = color_msg.blue();
      color.alpha = color_msg.alpha();
      gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(obj), &color);
      return;
    }

    case tangent::proto::ModelVariant::kPanzoomValue: {
      if (!GTK_IS_PANZOOM_AREA(obj)) {
        std::cerr << "WARNING: expected panzoom area for " << msg.name()
                  << " but got something else (" << G_OBJECT_TYPE(obj) << ")";
        return;
      }
      GtkPanZoomArea* panzoom = GTK_PANZOOM_AREA(obj);
      double offset[2] = {0, 0};
      const auto& panzoom_msg = msg.value().panzoom_value();
      offset[0] = panzoom_msg.offset_x();
      offset[1] = panzoom_msg.offset_y();
      gtk_panzoom_area_set_offset(panzoom, offset);
      gtk_panzoom_area_set_scale(panzoom, panzoom_msg.scale());
      gtk_panzoom_area_set_scale_rate(panzoom, panzoom_msg.scale_rate());
      return;
    }

    default: {
    }
  }
}

void deserialize_models(const tangent::proto::UIState& msg,
                        GtkBuilder* builder) {
  for (size_t idx = 0; idx < msg.entry_size(); idx++) {
    const auto& entry = msg.entry(idx);
    deserialize_object(entry, builder);
  }
}
