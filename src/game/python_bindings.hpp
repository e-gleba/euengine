#pragma once

#include "scene.hpp"

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace python_scripting
{

void init_python();
void shutdown_python();
void bind_scene_api();
void run_script(const std::string& script_path);
void update_scripts(float elapsed, float delta);
void set_context(egen::engine_context* ctx);

} // namespace python_scripting
