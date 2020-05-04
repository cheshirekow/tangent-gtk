set(_sources colormap.cc gdkcairo.c gdkcairomm.cc panzoomarea.c panzoomview.cc
             serializemodels.cc)
set(_pkgdeps eigen3 glib-2.0 gtk+-3.0 gtkmm-3.0 tinyxml2)

cc_library(
  tangent-gtk STATIC ${_sources}
  DEPS tangent::json fmt::fmt re2
  PKGDEPS ${_pkgdeps}
  PROPERTIES OUTPUT_NAME tangent-gtk)

cc_library(
  tangent-gtk-shared SHARED ${_sources}
  DEPS tangent::json-shared fmt::fmt re2-shared
  PKGDEPS ${_pkgdeps}
  PROPERTIES OUTPUT_NAME tangent-gtk)

if(NOT ${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  set(_copyfiles)
  foreach(gladefile panzoom.ui panzoom-base.ui serialize.ui tangent-gtk.xml)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${gladefile}
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${gladefile}
              ${CMAKE_CURRENT_BINARY_DIR}/${gladefile}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    list(APPEND _copyfiles ${CMAKE_CURRENT_BINARY_DIR}/${gladefile})
  endforeach()
endif()

add_custom_target(copyfiles-gtkutil ALL DEPENDS ${_copyfiles})

# NOTE(josh): Can do something like the following to edit gladefiles with this
# custom glade library
#
# ~~~
# add_custom_target(
#   edit.foo-ui
#   COMMAND
#     env
#     GLADE_CATALOG_SEARCH_PATH=${CMAKE_BINARY_DIR}/tangent/gtkutil
#     GLADE_MODULE_SEARCH_PATH=${CMAKE_BINARY_DIR}/tangent/gtkutil
#     glade gui.glade
#   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#   DEPENDS
#     ${CMAKE_BINARY_DIR}/tangent/gtkutil/tangent-gtk.xml
#     tangent-gtk-shared)
# ~~~

cc_binary(
  gtk-serialize-demo
  SRCS serializedemo.cc
  DEPS argue::static fmt::fmt tangent-gtk)

add_test(
  NAME tangent-gtk_serialize_test
  COMMAND python -B serialize_test.py ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(tangent-gtk_serialize_test PROPERTIES RUN_SERIAL TRUE)

cc_binary(
  gtk-panzoom-demo
  SRCS panzoomdemo.cc
  DEPS argue::static fmt::fmt phash tangent-gtk)

add_test(
  NAME tangent-gtk_panzoom_test
  COMMAND python -B panzoom_test.py ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(tangent-gtk_panzoom_test PROPERTIES RUN_SERIAL TRUE)

add_custom_target(
  edit.panzoom-base.ui
  COMMAND
    env GLADE_CATALOG_SEARCH_PATH=${CMAKE_BINARY_DIR}/tangent/gtkutil
    GLADE_MODULE_SEARCH_PATH=${CMAKE_BINARY_DIR}/tangent/gtkutil glade
    panzoom-base.ui
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tangent-gtk.xml tangent-gtk-shared)

add_custom_target(
  edit.panzoom.ui
  COMMAND
    env GLADE_CATALOG_SEARCH_PATH=${CMAKE_BINARY_DIR}/tangent/gtkutil
    GLADE_MODULE_SEARCH_PATH=${CMAKE_BINARY_DIR}/tangent/gtkutil glade
    panzoom.ui
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tangent-gtk.xml tangent-gtk-shared)

cc_test(
  gtkutil-colormap_test
  SRCS colormap_test.cc
  DEPS gtest gtest_main tangent-gtk
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/messages.pb.h
         ${CMAKE_CURRENT_BINARY_DIR}/messages.pb.cc
  COMMAND protoc --proto_path=${CMAKE_SOURCE_DIR} --cpp_out=${CMAKE_BINARY_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/messages.proto
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS messages.proto)

add_custom_target(
  gtkutil-genfiles DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/messages.pb.h
                           ${CMAKE_CURRENT_BINARY_DIR}/messages.pb.cc)

cc_library(
  gtkutil-protobuf STATIC
  SRCS ${CMAKE_CURRENT_BINARY_DIR}/messages.pb.h
       ${CMAKE_CURRENT_BINARY_DIR}/messages.pb.cc protobuf_serialize.cc
  DEPS tangent-gtk
  PKGDEPS protobuf ${_pkgdeps})
target_include_directories(gtkutil-protobuf PUBLIC ${CMAKE_BINARY_DIR})

cc_binary(
  gtkutil-genextradefs
  SRCS gen_extra_defs.cc
  DEPS tangent-gtk glibmm_generate_extra_defs-2.4
  PKGDEPS glibmm-2.4)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tangent_signals.defs
  COMMAND $<TARGET_FILE:gtkutil-genextradefs> >
          ${CMAKE_CURRENT_SOURCE_DIR}/tangent_signals.defs
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/mm/panzoomarea.h
         ${CMAKE_CURRENT_SOURCE_DIR}/mm/panzoomarea.cc
         ${CMAKE_CURRENT_SOURCE_DIR}/mm/private/panzoomarea_p.h
  COMMAND env M4PATH=/usr/lib/x86_64-linux-gnu/glibmm-2.4/proc/m4:/usr/lib/x86_64-linux-gnu/atkmm-1.6/proc/m4:/usr/lib/x86_64-linux-gnu/pangomm-1.4/proc/m4
          /usr/lib/x86_64-linux-gnu/glibmm-2.4/proc/gmmproc
          -I /usr/lib/x86_64-linux-gnu/gtkmm-3.0/proc/m4
          panzoomarea ${CMAKE_CURRENT_SOURCE_DIR}/mm ${CMAKE_CURRENT_SOURCE_DIR}/mm
  COMMAND
    clang-format-8 -i #
    ${CMAKE_CURRENT_SOURCE_DIR}/mm/panzoomarea.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mm/panzoomarea.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/mm/private/panzoomarea_p.h
  DEPENDS tangent_enums.defs
          tangent_vfuncs.defs
          tangent_methods.defs
          tangent_docs.xml
          tangent.defs
          tangent_signals.defs
          mm/panzoomarea.hg
          mm/panzoomarea.ccg
          mm/private/panzoomarea_p.hg
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/mm/wrap_init.cc
  COMMAND
    perl /usr/lib/x86_64-linux-gnu/glibmm-2.4/proc/generate_wrap_init.pl
    --namespace=Gtk --parent_dir=tangent/gtkutil/mm mm/panzoomarea.hg >
    ${CMAKE_CURRENT_SOURCE_DIR}/mm/wrap_init.cc
  COMMAND clang-format-8 -i ${CMAKE_CURRENT_SOURCE_DIR}/mm/wrap_init.cc
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS mm/panzoomarea.hg mm/panzoomarea.ccg)

cc_library(
  tangent-gtkmm
  SRCS ${CMAKE_CURRENT_SOURCE_DIR}/mm/panzoomarea.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mm/panzoomarea.cc
       ${CMAKE_CURRENT_SOURCE_DIR}/mm/private/panzoomarea_p.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mm/wrap_init.cc
  DEPS tangent-gtk
  PKGDEPS gtkmm-3.0)

cc_binary(
  gtk-panzoom-demo-mm
  SRCS mm/panzoomdemo.cc
  DEPS argue::static fmt::fmt phash tangent-gtkmm)
