cmake_minimum_required(VERSION 3.5)
project(tangentsky)
enable_testing()
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX
      ${CMAKE_BINARY_DIR}/install
      CACHE PATH "cmake installation directory")
endif()

find_package(Threads REQUIRED)
find_package(fmt)

# NOTE(josh): order matters
set(_tangent_modules
    environment.cmake
    wrappers.cmake
    codestyle.cmake
    ctest_helpers.cmake
    debian.cmake
    doctools.cmake
    pkgconfig.cmake
    re2-config.cmake)
foreach(module ${_tangent_modules})
  include(${CMAKE_SOURCE_DIR}/cmake/${module})
endforeach()

detect_buildenv()
if(TANGENT_SPARSE_EXPORT)
  message(STATUS "Detected sparse export: ${TANGENT_SPARSE_EXPORT}")
endif()

if(NOT TANGENT_BUILD_CONTEXT)
  set(TANGENT_BUILD_CONTEXT
      WORKSTATION
      CACHE STRING "what is the context for the current build")
endif()

if(TANGENT_SPARSE_EXPORT OR TANGENT_IS_DEBIAN_BUILD)
  # NOTE(josh): The order must be topological in dependency graph
  foreach(exportname libtangent-util libtangent-json argue)
    find_package(${exportname} CONFIG)
  endforeach()
endif()

# cmake-lint: disable=E1122
pkg_find(
  PKG eigen3
  PKG fontconfig
  PKG freetype2
  PKG fuse
  PKG glib-2.0
  PKG gnuradio-osmosdr
  PKG gnuradio-filter
  PKG glibmm-2.4
  PKG gtk+-3.0
  PKG gtkmm-3.0
  PKG libcurl
  PKG libelf
  PKG libglog
  PKG libpng NAMES libpng12 libpng16
  PKG librtlsdr
  PKG libudev
  PKG openssl
  PKG protobuf
  PKG tinyxml2
  PKG vulkan
  PKG x11-xcb)

# TODO(josh): can we get rid of these with some interface libraries?
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/third_party/googletest/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/glm)

set(CXX_STANDARD
    "c++11"
    CACHE STRING "argument to '-std=' for C++ compiler")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=${CXX_STANDARD}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

add_custom_target(chkfmt)
add_custom_target(doc)
add_custom_target(format)
add_custom_target(gen)
add_custom_target(lint)
add_custom_target(test-deps)
add_custom_target(wheels)

set(ENV{PYTHONPATH} ${CMAKE_SOURCE_DIR})
set(CTEST_ENVIRONMENT "PYTHONPATH=${CMAKE_SOURCE_DIR}")

# Will be populated by subdirectory listfiles with the dependencies of the
# master sphinx build
set_property(GLOBAL PROPERTY global_doc_files "")

add_custom_command(
  OUTPUT __always_rebuild
  COMMAND cmake -E echo "" > /dev/null
  COMMENT "Stubbing __always_rebuild")

add_custom_target(
  check-codestyle-manifest ALL
  COMMAND
    python -Bsm tangent.tooling.generate_style_manifest #
    --outfile ${CMAKE_BINARY_DIR}/codestyle_manifest.cmake #
    --excludes-from ${CMAKE_SOURCE_DIR}/cmake/lint-excludes.txt #
    -- ${CMAKE_SOURCE_DIR}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS __always_rebuild
  COMMENT "Checking for changes in codestyle manifest")

check_call(
  COMMAND
    python -Bsm tangent.tooling.generate_style_manifest #
    --outfile ${CMAKE_BINARY_DIR}/codestyle_manifest.cmake #
    --excludes-from ${CMAKE_SOURCE_DIR}/cmake/lint-excludes.txt #
    -- ${CMAKE_SOURCE_DIR}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# If the manifest changes, then we need to rerun cmake to import link/fmt rules
# for the new manifest
configure_file(${CMAKE_BINARY_DIR}/codestyle_manifest.cmake
               ${CMAKE_BINARY_DIR}/codestyle_manifest.stamp COPYONLY)

include(${CMAKE_BINARY_DIR}/codestyle_manifest.cmake)

# Create python environment with packages needed to do the build
set(PYENV_WITNESS ${CMAKE_BINARY_DIR}/pyenv.witness)
set(PYENV
    ${CMAKE_COMMAND} -E env #
    VIRTUAL_ENV=${CMAKE_BINARY_DIR}/pyenv #
    "PATH=${CMAKE_BINARY_DIR}/pyenv:\$\${PATH}")

if(CMAKE_GENERATOR STREQUAL "Ninja")
  set(_depfile DEPFILE ${CMAKE_BINARY_DIR}/pyenv.d)
endif()

add_custom_command(
  OUTPUT ${PYENV_WITNESS}
  COMMAND rm -rf ${CMAKE_BINARY_DIR}/pyenv
  COMMAND virtualenv -p python3 ${CMAKE_BINARY_DIR}/pyenv
  COMMAND ${PYENV} pip install -r requirements.txt
  COMMAND echo "pyenv.witness : \\\\" > ${CMAKE_BINARY_DIR}/pyenv.d
  COMMAND
    env LC_ALL=C #
    find ${CMAKE_BINARY_DIR}/pyenv -type l,f #
    | sed 's: :\\\\ :g' #
    | sort #
    | sed 's:$$:\\\\:' >> ${CMAKE_BINARY_DIR}/pyenv.d
  COMMAND touch ${PYENV_WITNESS}
  DEPENDS requirements.txt
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Creating python environment" ${_depfile})

# NOTE(josh): bug in cmake 3.10. If we don't create this custom target, then
# cmake wont emit the rule... even though we have a rule later on that depends
# on the outputs of the custom command.
add_custom_target(pyenv DEPENDS ${PYENV_WITNESS})

# NOTE(josh): search through the list of child directories and add any that
# actually contain a listfile. While globs are evil, this is necessary for
# sparse checkouts. We can and should correctly add dependencies for this glob
# in order to retrigger cmake.
file(
  GLOB children
  RELATIVE ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/*)
foreach(child ${children})
  if(EXISTS ${CMAKE_SOURCE_DIR}/${child}/CMakeLists.txt)
    message("Enabling subdirectory '${child}' for this checkout")
    add_subdirectory(${child})
  endif()
endforeach()

# NOTE(josh): some sparse checkouts don't include doxygen
if(EXISTS ${CMAKE_SOURCE_DIR}/doxy.config.in)
  # configure the doxygen configuration
  # NOTE(josh): maybe want to expose this for editor integration
  # ~~~
  # set(DOXY_WARN_FORMAT "\"$file($line) : $text \"")
  # set(DOXY_WARN_FORMAT "\"$file:$line: $text \"")
  # ~~~
  configure_file("${PROJECT_SOURCE_DIR}/doxy.config.in"
                 "${PROJECT_BINARY_DIR}/doxy.config")

  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/doxy.stamp
    COMMAND doxygen ${PROJECT_BINARY_DIR}/doxy.config
    COMMAND touch ${PROJECT_BINARY_DIR}/doxy.stamp
    DEPENDS ${PROJECT_BINARY_DIR}/doxy.config
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_custom_target(doxygen DEPENDS ${PROJECT_BINARY_DIR}/doxy.stamp)
  add_dependencies(doc doxygen)
endif()

add_custom_target(
  better-test
  DEPENDS test-deps
  COMMAND ctest --force-new-ctest-process --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Execute ctest")

add_custom_target(
  buildbot-test
  DEPENDS test-deps
  COMMAND
    # cmake-format: off
    ctest
    --label-exclude CI_DISABLED
    --force-new-ctest-process
    --output-on-failure
    --output-log ${CMAKE_BINARY_DIR}/ctestlog.txt
    # cmake-format: on
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_target(pre-push)
add_dependencies(pre-push lint chkfmt)

add_custom_target(pre-release)
add_dependencies(pre-release better-test doc lint chkfmt)


# Inform vscode where compile commands is
foreach(filename c_cpp_properties.json tasks.json)
  file(READ ${CMAKE_SOURCE_DIR}/.vscode/${filename} _content)
  string(REPLACE "\${cmakeBinaryDir}" "${CMAKE_BINARY_DIR}" _content
                 "${_content}")
  file(WRITE ${CMAKE_SOURCE_DIR}/.vscode/${filename} "${_content}")
endforeach()
