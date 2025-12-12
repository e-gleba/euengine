#include "python_bindings.hpp"
#include "ui.hpp"

#include <core-api/game.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <spdlog/spdlog.h>

namespace python_scripting
{

namespace
{

bool                                    g_python_initialized = false;
std::unique_ptr<py::scoped_interpreter> g_python_interpreter;

// Global context pointer for Python callbacks
euengine::engine_context* g_ctx = nullptr;

// Python module instance
py::module_ g_scene_module;

// Create the embedded module using PYBIND11_EMBEDDED_MODULE
PYBIND11_EMBEDDED_MODULE(euengine_scene, m)
{
    (void)m; // Module will be populated in bind_scene_api()
    // We just create the module structure here
}

// Helper to convert glm::vec3 to Python tuple
py::tuple vec3_to_tuple(const glm::vec3& v)
{
    return py::make_tuple(v.x, v.y, v.z);
}

// Helper to convert Python tuple/list to glm::vec3
glm::vec3 tuple_to_vec3(const py::object& obj)
{
    if (py::isinstance<py::tuple>(obj) || py::isinstance<py::list>(obj))
    {
        auto seq = py::cast<py::sequence>(obj);
        if (py::len(seq) >= 3)
        {
            return glm::vec3(py::cast<float>(seq[0]),
                             py::cast<float>(seq[1]),
                             py::cast<float>(seq[2]));
        }
    }
    return glm::vec3(0.0f);
}

} // namespace

void bind_scene_api()
{
    if (!g_python_initialized)
    {
        return;
    }

    // Import the embedded module we created with PYBIND11_EMBEDDED_MODULE
    g_scene_module = py::module_::import("euengine_scene");

    // Bind glm::vec3
    py::class_<glm::vec3>(g_scene_module, "Vec3")
        .def(py::init<>())
        .def(py::init<float, float, float>())
        .def_readwrite("x", &glm::vec3::x)
        .def_readwrite("y", &glm::vec3::y)
        .def_readwrite("z", &glm::vec3::z)
        .def("__repr__",
             [](const glm::vec3& v)
             {
                 return "Vec3(" + std::to_string(v.x) + ", " +
                        std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
             });

    // Bind transform
    py::class_<euengine::transform>(g_scene_module, "Transform")
        .def(py::init<>())
        .def_readwrite("position", &euengine::transform::position)
        .def_readwrite("rotation", &euengine::transform::rotation)
        .def_readwrite("scale", &euengine::transform::scale);

    // Scene API functions
    g_scene_module.def(
        "add_model",
        [](const std::string& path, const py::object& pos_obj, float scale)
        {
            glm::vec3 pos   = tuple_to_vec3(pos_obj);
            auto*     model = scene::add_model(path, pos, scale);
            return model != nullptr;
        },
        py::arg("path"),
        py::arg("position") = py::make_tuple(0.0f, 0.0f, 0.0f),
        py::arg("scale")    = 1.0f,
        "Add a model to the scene");

    g_scene_module.def(
        "remove_model",
        [](int idx) { scene::remove_model(idx); },
        py::arg("index"),
        "Remove a model from the scene");

    g_scene_module.def(
        "get_model_count",
        []() { return static_cast<int>(scene::g_models.size()); },
        "Get the number of models in the scene");

    g_scene_module.def(
        "get_model_position",
        [](int idx) -> py::tuple
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return py::make_tuple(0.0f, 0.0f, 0.0f);
            }
            return vec3_to_tuple(scene::g_models[static_cast<std::size_t>(idx)]
                                     .transform.position);
        },
        py::arg("index"),
        "Get model position");

    g_scene_module.def(
        "set_model_position",
        [](int idx, const py::object& pos_obj)
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return;
            }
            glm::vec3 pos = tuple_to_vec3(pos_obj);
            scene::g_models[static_cast<std::size_t>(idx)].transform.position =
                pos;
        },
        py::arg("index"),
        py::arg("position"),
        "Set model position");

    g_scene_module.def(
        "set_model_rotation",
        [](int idx, const py::object& rot_obj)
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return;
            }
            glm::vec3 rot = tuple_to_vec3(rot_obj);
            scene::g_models[static_cast<std::size_t>(idx)].transform.rotation =
                rot;
        },
        py::arg("index"),
        py::arg("rotation"),
        "Set model rotation (Euler angles in degrees)");

    g_scene_module.def(
        "set_model_scale",
        [](int idx, const py::object& scale_obj)
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return;
            }
            glm::vec3 scale = tuple_to_vec3(scale_obj);
            scene::g_models[static_cast<std::size_t>(idx)].transform.scale =
                scale;
        },
        py::arg("index"),
        py::arg("scale"),
        "Set model scale");

    g_scene_module.def(
        "set_model_color_tint",
        [](int idx, const py::object& color_obj)
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return;
            }
            glm::vec3 color = tuple_to_vec3(color_obj);
            scene::g_models[static_cast<std::size_t>(idx)].color_tint = color;
        },
        py::arg("index"),
        py::arg("color"),
        "Set model color tint (RGB, 0-1 range)");

    g_scene_module.def(
        "enable_model_animation",
        [](int idx, bool enable, float speed = 25.0f)
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return;
            }
            scene::g_models[static_cast<std::size_t>(idx)].animate    = enable;
            scene::g_models[static_cast<std::size_t>(idx)].anim_speed = speed;
        },
        py::arg("index"),
        py::arg("enable"),
        py::arg("speed") = 25.0f,
        "Enable/disable model rotation animation");

    g_scene_module.def(
        "enable_model_hover",
        [](int idx, bool enable, float speed = 1.5f, float range = 0.2f)
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return;
            }
            auto& model       = scene::g_models[static_cast<std::size_t>(idx)];
            model.hover       = enable;
            model.hover_speed = speed;
            model.hover_range = range;
            if (enable)
            {
                model.hover_base = model.transform.position.y;
            }
        },
        py::arg("index"),
        py::arg("enable"),
        py::arg("speed") = 1.5f,
        py::arg("range") = 0.2f,
        "Enable/disable model hover animation");

    g_scene_module.def(
        "enable_model_movement",
        [](int               idx,
           bool              enable,
           const py::object& start_obj,
           const py::object& end_obj,
           float             speed = 0.5f)
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return;
            }
            auto& model      = scene::g_models[static_cast<std::size_t>(idx)];
            model.moving     = enable;
            model.move_speed = speed;
            if (enable)
            {
                model.move_start = tuple_to_vec3(start_obj);
                model.move_end   = tuple_to_vec3(end_obj);
                model.move_path  = 0.0f;
                model.move_dir   = 1.0f;
            }
        },
        py::arg("index"),
        py::arg("enable"),
        py::arg("start"),
        py::arg("end"),
        py::arg("speed") = 0.5f,
        "Enable/disable model movement along a path");

    g_scene_module.def(
        "get_model_name",
        [](int idx) -> std::string
        {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= scene::g_models.size())
            {
                return "";
            }
            return scene::g_models[static_cast<std::size_t>(idx)].name;
        },
        py::arg("index"),
        "Get model name");

    g_scene_module.def(
        "focus_camera_on_model",
        [](int idx) { scene::focus_camera_on_object(idx); },
        py::arg("index"),
        "Focus camera on a model");

    g_scene_module.def(
        "log",
        [](const std::string& msg) { ui::log(2, msg); },
        py::arg("message"),
        "Log a message to the console");

    g_scene_module.def(
        "get_time",
        []() -> float
        {
            if (g_ctx != nullptr)
            {
                return g_ctx->time.elapsed;
            }
            return 0.0f;
        },
        "Get elapsed time in seconds");

    g_scene_module.def(
        "get_delta_time",
        []() -> float
        {
            if (g_ctx != nullptr)
            {
                return g_ctx->time.delta;
            }
            return 0.0f;
        },
        "Get delta time in seconds");

    g_scene_module.def(
        "get_fps",
        []() -> float
        {
            if (g_ctx != nullptr)
            {
                return g_ctx->time.fps;
            }
            return 0.0f;
        },
        "Get current FPS");

    // Module is already in sys.modules from above
}

void init_python()
{
    if (g_python_initialized)
    {
        return;
    }

    try
    {
        g_python_interpreter = std::make_unique<py::scoped_interpreter>();
        g_python_initialized = true;
        spdlog::info("Python interpreter initialized");

        bind_scene_api();
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to initialize Python: {}", e.what());
        g_python_initialized = false;
        g_python_interpreter.reset();
    }
}

void shutdown_python()
{
    if (!g_python_initialized)
    {
        return;
    }

    try
    {
        // Clear module reference first to release any Python objects
        g_scene_module = py::module_();

        // Clear the interpreter - this will properly finalize Python
        // The unique_ptr will automatically call the destructor which handles
        // cleanup
        g_python_interpreter.reset();
        g_python_initialized = false;
        spdlog::info("Python interpreter shutdown");
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error shutting down Python: {}", e.what());
        g_python_initialized = false;
        g_python_interpreter.reset();
    }
    catch (...)
    {
        // Catch any Python exceptions during shutdown
        spdlog::error("Unknown error during Python shutdown");
        g_python_initialized = false;
        g_python_interpreter.reset();
    }
}

void run_script(const std::string& script_path)
{
    if (!g_python_initialized)
    {
        spdlog::warn("Python not initialized, cannot run script: {}",
                     script_path);
        return;
    }

    if (!std::filesystem::exists(script_path))
    {
        spdlog::warn("Script file not found: {}", script_path);
        return;
    }

    try
    {
        py::module_ sys = py::module_::import("sys");
        sys.attr("path").attr("insert")(
            0, std::filesystem::path(script_path).parent_path().string());

        py::eval_file(script_path);
        spdlog::info("Executed Python script: {}", script_path);
    }
    catch (const py::error_already_set& e)
    {
        spdlog::error("Python script error in {}: {}", script_path, e.what());
    }
    catch (const std::exception& e)
    {
        spdlog::error(
            "Error running Python script {}: {}", script_path, e.what());
    }
}

void update_scripts(float elapsed, float delta)
{
    if (!g_python_initialized)
    {
        return;
    }

    try
    {
        // Check if there's an update function in the main module
        auto main_module = py::module_::import("__main__");
        if (py::hasattr(main_module, "update"))
        {
            auto update_func = main_module.attr("update");
            if (py::isinstance<py::function>(update_func))
            {
                update_func(elapsed, delta);
            }
        }
    }
    catch (const py::error_already_set& e)
    {
        // Silently ignore if update function doesn't exist
        // This is expected for scripts that don't have an update function
    }
    catch (const std::exception& e)
    {
        // Log other errors but don't crash
        spdlog::debug("Python update error: {}", e.what());
    }
}

void set_context(euengine::engine_context* ctx)
{
    g_ctx = ctx;
}

} // namespace python_scripting
