#include "ui.hpp"

// Suppress deprecated warnings for legacy context members
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "imgui_style.hpp"
#include "scene.hpp"

#include <core-api/camera.hpp>
#include <core-api/profiler.hpp>
#include <imgui.h>
#include <implot.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>
#include <utility>

namespace ui
{

namespace
{

// Sanitize UTF-8 string to avoid Pango warnings
std::string sanitize_utf8(const std::string& str)
{
    std::string result;
    result.reserve(str.size());

    for (std::size_t i = 0; i < str.size(); ++i)
    {
        auto c = static_cast<unsigned char>(str[i]);

        // Valid ASCII
        if (c < 0x80)
        {
            result += static_cast<char>(c);
        }
        // Valid UTF-8 continuation or start
        else if ((c & 0xE0) == 0xC0 && i + 1 < str.size())
        {
            // 2-byte sequence
            auto c2 = static_cast<unsigned char>(str[i + 1]);
            if ((c2 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                ++i;
            }
            else
            {
                result += '?'; // Invalid, replace
            }
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < str.size())
        {
            // 3-byte sequence
            auto c2 = static_cast<unsigned char>(str[i + 1]);
            auto c3 = static_cast<unsigned char>(str[i + 2]);
            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                result += static_cast<char>(c3);
                i += 2;
            }
            else
            {
                result += '?';
            }
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < str.size())
        {
            // 4-byte sequence
            auto c2 = static_cast<unsigned char>(str[i + 1]);
            auto c3 = static_cast<unsigned char>(str[i + 2]);
            auto c4 = static_cast<unsigned char>(str[i + 3]);
            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 &&
                (c4 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                result += static_cast<char>(c3);
                result += static_cast<char>(c4);
                i += 3;
            }
            else
            {
                result += '?';
            }
        }
        else
        {
            result += '?'; // Invalid byte, replace with ?
        }
    }

    return result;
}

// Modern Steam 2024+ inspired theme - clean, flat, professional
// Load a glTF/GLB scene file
void load_gltf_glb_scene(const std::filesystem::path& path)
{
    std::string filename = sanitize_utf8(path.filename().string());
    auto        ext      = path.extension().string();
    std::ranges::transform(ext, ext.begin(), ::tolower);
    std::string file_type = (ext == ".glb") ? "GLB" : "glTF";

    log(2, "Loading " + file_type + " scene: " + filename);

    // Load model at origin with default scale
    // Engine's glTF loader handles hierarchy and transforms internally
    if (auto* m = scene::add_model(path.string(), { 0.0f, 0.0f, 0.0f }, 1.0f))
    {
        m->name = path.stem().string();
        log(2, "Successfully loaded " + file_type + " scene: " + filename);
        g_show_file_dialog = false;
        g_file_dialog_selected_file.clear();
    }
    else
    {
        log(4, "Failed to load " + file_type + " scene: " + filename);
    }
}

void draw_menu(euengine::engine_context* ctx)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Hot Reload", "F5"))
            {
                log(2, "Shader hot reload triggered");
                if (ctx->shader_system != nullptr)
                {
                    ctx->shader_system->enable_hot_reload(false);
                    ctx->shader_system->enable_hot_reload(true);
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Reload shaders and game module");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Load Scene...", "Ctrl+O"))
            {
                g_show_file_dialog = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Load a glTF/GLB scene file");
            }
            if (ImGui::MenuItem("Clear Scene", "Ctrl+N"))
            {
                // Remove all models except keep camera
                while (!scene::g_models.empty())
                {
                    scene::remove_model(0);
                }
                log(2, "Scene cleared");
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Remove all objects from the scene");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rescan Assets"))
            {
                scene::scan_models();
                scene::scan_audio();
                scene::scan_scenes();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Refresh asset lists from disk");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4"))
            {
                ctx->settings->request_quit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Scene", nullptr, &g_show_hierarchy);
            ImGui::MenuItem("Inspector", nullptr, &g_show_inspector);
            ImGui::MenuItem("Asset Browser", nullptr, &g_show_browser);
            ImGui::MenuItem("Audio Player", nullptr, &g_show_audio);
            ImGui::Separator();
            ImGui::MenuItem("Engine Settings", nullptr, &g_show_engine);
            ImGui::MenuItem("Performance", nullptr, &g_show_stats);
            ImGui::MenuItem("Console", "`", &g_show_console);
            ImGui::Separator();
            if (ImGui::MenuItem("Keyboard Shortcuts...", "F1"))
            {
                g_show_shortcuts = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("View all available keyboard shortcuts");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Window Layout"))
            {
                // Set flag to reset all window positions on next frame
                g_reset_window_layout = true;

                // Also clear INI file for future sessions
                ImGuiIO& io = ImGui::GetIO();
                if (io.IniFilename != nullptr)
                {
                    try
                    {
                        std::filesystem::path ini_path = io.IniFilename;
                        if (std::filesystem::exists(ini_path))
                        {
                            std::filesystem::remove(ini_path);
                        }
                    }
                    catch (const std::exception& e)
                    {
                        log(4,
                            std::format("Failed to delete INI file: {}",
                                        e.what()));
                    }
                }
                log(2, "Window layout will reset on next frame");
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "Reset all window positions and sizes to defaults");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Render"))
        {
            ImGui::MenuItem("Wireframe Mode", "Tab", &g_wireframe);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Toggle wireframe rendering mode");
            }
            ImGui::MenuItem("Auto Animate", "Space", &g_auto_rotate);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Toggle automatic object animation");
            }
            ImGui::Separator();
            if (ImGui::ColorEdit3("Background",
                                  g_sky_color,
                                  ImGuiColorEditFlags_NoInputs |
                                      ImGuiColorEditFlags_NoLabel))
            {
                scene::apply_sky();
            }
            ImGui::SameLine();
            ImGui::Text("Background Color");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Change the scene background color");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Keyboard Shortcuts", "F1"))
            {
                g_show_shortcuts = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("View all keyboard shortcuts");
            }
            ImGui::EndMenu();
        }

        // Right: Stats
        auto  info      = std::format("{:.0f} FPS  |  {}x{}  |  {} objects",
                                ctx->time.fps,
                                ctx->settings->get_window_width(),
                                ctx->settings->get_window_height(),
                                scene::g_models.size());
        auto  safe_info = sanitize_utf8(info);
        float w         = ImGui::CalcTextSize(safe_info.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - 20);
        ImGui::TextColored(
            ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", safe_info.c_str());

        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();
}

void draw_scene(euengine::engine_context* ctx)
{
    if (!g_show_hierarchy)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(16, 40),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 600),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 300),
                                        ImVec2(500, io.DisplaySize.y - 60));

    if (ImGui::Begin("Scene", &g_show_hierarchy))
    {
        // Camera section
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (scene::g_camera != entt::null &&
                ctx->registry->valid(scene::g_camera))
            {
                auto& cam = ctx->registry->get<euengine::camera_component>(
                    scene::g_camera);

                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
                ImGui::Text("Position");
                ImGui::PopStyleColor();
                ImGui::DragFloat3("##pos", &cam.position.x, 0.2f);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Camera position in world space\nDrag to "
                                      "adjust or use WASD to move");
                }

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
                ImGui::Text("Settings");
                ImGui::PopStyleColor();
                ImGui::SliderFloat("Speed", &cam.move_speed, 1.0f, 50.0f);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "Camera movement speed\nHold Shift to move faster");
                }
                ImGui::SliderFloat("FOV", &cam.fov, 30.0f, 120.0f);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Field of view angle in degrees");
                }

                ImGui::Spacing();
                if (ImGui::Button("Reset Camera", ImVec2(-1, 28)))
                {
                    cam.position = { 0.0f, 10.0f, 25.0f };
                    cam.pitch    = -15.0f;
                    cam.yaw      = 0.0f;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "Reset camera to default position and rotation");
                }
            }
        }

        // Environment section
        if (ImGui::CollapsingHeader("Environment",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Colors");
            ImGui::PopStyleColor();

            if (ImGui::ColorEdit3("Sky", g_sky_color))
            {
                scene::apply_sky();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Background sky color");
            }
            if (ImGui::ColorEdit3("Grid", g_grid_color))
            {
                scene::rebuild_grid();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Grid line color");
            }

            ImGui::Spacing();
            ImGui::Checkbox("Origin Axis", &scene::g_show_origin);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Show X/Y/Z axis lines at world origin");
            }
        }

        // Objects list
        if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Header with count and quick actions
            ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text(
                "%s", std::format("{} items", scene::g_models.size()).c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
            if (ImGui::SmallButton("+ Add"))
            {
                g_show_browser = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Open Asset Browser to add objects");
            }
            ImGui::EndGroup();

            ImGui::Spacing();

            // Enhanced search with clear button
            static char obj_filter[128] = {};
            ImGui::BeginGroup();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
            ImGui::InputTextWithHint("##obj_filter",
                                     "Search objects...",
                                     obj_filter,
                                     sizeof(obj_filter));
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Filter objects by name");
            }
            if (obj_filter[0] != '\0')
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                {
                    obj_filter[0] = '\0';
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Clear search filter");
                }
            }
            ImGui::EndGroup();

            // Clear selection button
            if (scene::g_selected >= 0 || !scene::g_selected_set.empty())
            {
                ImGui::Spacing();
                if (ImGui::SmallButton("Clear Selection"))
                {
                    scene::g_selected = -1;
                    scene::g_selected_set.clear();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "Deselect all objects\nCtrl+Click to multi-select");
                }
            }

            ImGui::Spacing();
            ImGui::BeginChild("##objs", ImVec2(0, -1), 1);

            for (std::size_t i = 0; i < scene::g_models.size(); ++i)
            {
                auto& m = scene::g_models[i];

                // Apply filter
                if (obj_filter[0] != '\0' &&
                    m.name.find(obj_filter) == std::string::npos)
                {
                    continue;
                }

                bool sel =
                    (std::cmp_equal(i, scene::g_selected)) ||
                    (scene::g_selected_set.contains(static_cast<int>(i)));

                // Enhanced visual indicators
                ImVec4      col    = ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
                const char* icon   = "";
                const char* status = "";
                if (m.moving)
                {
                    col    = ImVec4(0.40f, 0.80f, 0.50f, 1.0f);
                    icon   = "[MOV] ";
                    status = "Moving";
                }
                else if (m.hover)
                {
                    col    = ImVec4(0.40f, 0.80f, 0.95f, 1.0f);
                    icon   = "[HOV] ";
                    status = "Hovering";
                }
                else if (m.animate)
                {
                    col    = ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
                    icon   = "[ROT] ";
                    status = "Rotating";
                }

                if (sel)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header,
                                          ImVec4(0.28f, 0.55f, 0.80f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                }

                auto safe_name = sanitize_utf8(m.name);
                auto label     = std::format("{}{}", icon, safe_name);

                // Multi-select support with Ctrl+Click
                bool ctrl_held = ImGui::GetIO().KeyCtrl;
                if (ImGui::Selectable(
                        label.c_str(),
                        sel,
                        ImGuiSelectableFlags_AllowOverlap |
                            (ctrl_held ? ImGuiSelectableFlags_DontClosePopups
                                       : 0)))
                {
                    int idx = static_cast<int>(i);
                    if (ctrl_held)
                    {
                        // Toggle selection in multi-select set
                        if (scene::g_selected_set.contains(idx))
                        {
                            scene::g_selected_set.erase(idx);
                            // If this was the primary selection, clear it
                            if (scene::g_selected == idx)
                            {
                                scene::g_selected =
                                    scene::g_selected_set.empty()
                                        ? -1
                                        : *scene::g_selected_set.begin();
                            }
                        }
                        else
                        {
                            scene::g_selected_set.insert(idx);
                            scene::g_selected = idx; // Set as primary selection
                        }
                    }
                    else
                    {
                        // Single select - clear multi-select and set new
                        // primary
                        scene::g_selected_set.clear();
                        scene::g_selected_set.insert(idx);
                        scene::g_selected = idx;
                    }
                }

                ImGui::PopStyleColor(2);

                // Enhanced tooltip with more info
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f),
                                       "%s",
                                       safe_name.c_str());
                    ImGui::Separator();
                    ImGui::Text("Position: %.2f, %.2f, %.2f",
                                m.transform.position.x,
                                m.transform.position.y,
                                m.transform.position.z);
                    ImGui::Text("Scale: %.2f", m.transform.scale.x);
                    if (status[0] != '\0')
                    {
                        ImGui::TextColored(col, "Status: %s", status);
                    }
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                                       "Double-click to focus "
                                       "camera\nCtrl+Click to multi-select");
                    ImGui::EndTooltip();
                }

                // Double-click to focus
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) &&
                    scene::g_camera != entt::null)
                {
                    auto& cam = ctx->registry->get<euengine::camera_component>(
                        scene::g_camera);
                    cam.position = m.transform.position + glm::vec3(0, 2, 5);
                    cam.pitch    = -15.0f;
                    cam.yaw      = -90.0f;
                    log(2, "Camera focused on: " + safe_name);
                }
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void draw_inspector()
{
    if (!g_show_inspector)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    // Position below Scene window (Scene is at y=40, height=600, so start at
    // 650)
    ImGui::SetNextWindowPos(ImVec2(16, 650),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 200),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 150),
                                        ImVec2(500, io.DisplaySize.y - 60));

    if (ImGui::Begin("Inspector", &g_show_inspector))
    {
        if (scene::g_selected >= 0 &&
            static_cast<std::size_t>(scene::g_selected) <
                scene::g_models.size())
        {
            auto& m =
                scene::g_models[static_cast<std::size_t>(scene::g_selected)];

            // Enhanced header
            auto safe_name = sanitize_utf8(m.name);
            auto safe_path = sanitize_utf8(
                std::filesystem::path(m.path).filename().string());
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.38f, 0.68f, 0.93f, 1.0f));
            ImGui::Text("%s", safe_name.c_str());
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("  %s", safe_path.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();

            // Transform section
            if (ImGui::CollapsingHeader("Transform",
                                        ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::DragFloat3(
                        "Position", &m.transform.position.x, 0.05f))
                {
                    // Apply grid snapping if enabled
                    if (g_grid_snap && g_snap_size > 0.0f)
                    {
                        m.transform.position.x =
                            std::round(m.transform.position.x / g_snap_size) *
                            g_snap_size;
                        m.transform.position.y =
                            std::round(m.transform.position.y / g_snap_size) *
                            g_snap_size;
                        m.transform.position.z =
                            std::round(m.transform.position.z / g_snap_size) *
                            g_snap_size;
                    }
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Object position in world space\nHold "
                                      "Ctrl for finer control");
                }

                if (ImGui::DragFloat3(
                        "Rotation", &m.transform.rotation.x, 0.5f))
                {
                    // Normalize rotation to 0-360 range
                    m.transform.rotation.x =
                        std::fmod(m.transform.rotation.x + 360.0f, 360.0f);
                    m.transform.rotation.y =
                        std::fmod(m.transform.rotation.y + 360.0f, 360.0f);
                    m.transform.rotation.z =
                        std::fmod(m.transform.rotation.z + 360.0f, 360.0f);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Rotation in degrees (X, Y, Z)");
                }

                float sc = m.transform.scale.x;
                if (ImGui::SliderFloat("Scale", &sc, 0.01f, 10.0f))
                {
                    m.transform.scale = glm::vec3(sc);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Uniform scale factor (0.01 - 10.0)");
                }
            }

            // Color tint (especially for duck)
            if (m.name.find("duck") != std::string::npos || m.name == "duck")
            {
                if (ImGui::CollapsingHeader("Material"))
                {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
                    ImGui::Text("Color Tint");
                    ImGui::PopStyleColor();

                    float color[3] = { m.color_tint.r,
                                       m.color_tint.g,
                                       m.color_tint.b };
                    if (ImGui::ColorEdit3("##tint", color))
                    {
                        m.color_tint = glm::vec3(color[0], color[1], color[2]);
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Color tint applied to the model");
                    }
                }
            }

            // Animation section
            if (ImGui::CollapsingHeader("Animation"))
            {
                ImGui::Checkbox("Rotate", &m.animate);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Enable continuous rotation animation");
                }
                if (m.animate)
                {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    ImGui::SliderFloat(
                        "##speed", &m.anim_speed, 0.0f, 100.0f, "%.0f deg/s");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip(
                            "Rotation speed in degrees per second");
                    }
                }
                ImGui::Checkbox("Hover", &m.hover);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Enable vertical hovering animation");
                }
            }

            ImGui::Separator();

            // Actions section
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
            ImGui::Text("Actions");
            ImGui::PopStyleColor();

            // Teleport to Camera button
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.40f, 0.70f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImVec4(0.50f, 0.80f, 0.50f, 1.0f));
            if (ImGui::Button("Teleport to Camera", ImVec2(-1, 28)))
            {
                scene::teleport_object_to_camera(scene::g_selected);
                log(2, "Teleported object to camera: " + safe_name);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Move this object to the camera position");
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            // Duplicate button
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.28f, 0.55f, 0.80f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImVec4(0.35f, 0.65f, 0.90f, 1.0f));
            if (ImGui::Button("Duplicate", ImVec2(-1, 28)))
            {
                scene::duplicate_model(scene::g_selected);
                log(2, "Duplicated object: " + safe_name);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Create a copy of this object");
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.70f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImVec4(0.85f, 0.35f, 0.35f, 1.0f));
            if (ImGui::Button("Delete Object", ImVec2(-1, 32)))
            {
                scene::remove_model(scene::g_selected);
                log(2, "Deleted object: " + safe_name);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Remove this object from the scene");
            }
            ImGui::PopStyleColor(2);
        }
        else
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                               "No object selected");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.48f, 1.0f),
                               "Select an object from the Scene\npanel to view "
                               "and edit\nits properties here.");
        }
    }
    ImGui::End();
}

void draw_browser()
{
    if (!g_show_browser)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300, 40),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(284, 360),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 200),
                                        ImVec2(450, io.DisplaySize.y - 60));

    if (ImGui::Begin("Asset Browser", &g_show_browser))
    {
        // Header with refresh button
        ImGui::BeginGroup();
        if (ImGui::Button("Refresh", ImVec2(80, 0)))
        {
            scene::scan_models();
            log(2, "Asset browser refreshed");
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Rescan for model files");
        }
        ImGui::SameLine();

        std::size_t visible_count =
            g_browser_filter.empty()
                ? scene::g_model_files.size()
                : static_cast<std::size_t>(std::ranges::count_if(
                      scene::g_model_files,
                      [](const auto& f)
                      {
                          auto name =
                              std::filesystem::path(f).filename().string();
                          return name.find(g_browser_filter) !=
                                 std::string::npos;
                      }));
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                           "%s",
                           std::format("{} models", visible_count).c_str());
        ImGui::EndGroup();

        ImGui::Separator();

        // Enhanced search filter with clear button
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
        char filter_buf[256] = {};
        std::strncpy(
            filter_buf, g_browser_filter.c_str(), sizeof(filter_buf) - 1);
        if (ImGui::InputTextWithHint("##browser_filter",
                                     "Search models...",
                                     filter_buf,
                                     sizeof(filter_buf)))
        {
            g_browser_filter = filter_buf;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Filter models by filename");
        }
        if (g_browser_filter[0] != '\0')
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
            {
                g_browser_filter.clear();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Clear search filter");
            }
        }
        ImGui::EndGroup();

        ImGui::Spacing();

        ImGui::BeginChild("##list", ImVec2(0, -40), 1);
        for (std::size_t i = 0; i < scene::g_model_files.size(); ++i)
        {
            auto name = std::filesystem::path(scene::g_model_files[i])
                            .filename()
                            .string();

            // Apply filter
            if (!g_browser_filter.empty() &&
                name.find(g_browser_filter) == std::string::npos)
            {
                continue;
            }

            auto safe_name = sanitize_utf8(name);
            bool sel       = std::cmp_equal(i, scene::g_browser_sel);

            // Show file extension as hint
            auto ext = std::filesystem::path(scene::g_model_files[i])
                           .extension()
                           .string();
            std::ranges::transform(ext, ext.begin(), ::tolower);

            if (ImGui::Selectable(safe_name.c_str(), sel))
            {
                scene::g_browser_sel = static_cast<int>(i);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextColored(
                    ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "%s", safe_name.c_str());
                ImGui::Text("Type: %s", ext.c_str());
                ImGui::Text("Path: %s",
                            sanitize_utf8(scene::g_model_files[i]).c_str());
                ImGui::EndTooltip();
            }
        }
        ImGui::EndChild();

        bool ok = scene::g_browser_sel >= 0 &&
                  static_cast<std::size_t>(scene::g_browser_sel) <
                      scene::g_model_files.size();

        ImGui::PushStyleColor(ImGuiCol_Button,
                              ok ? ImVec4(0.28f, 0.55f, 0.80f, 1.0f)
                                 : ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
        if (!ok)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Add to Scene", ImVec2(-1, 32)))
        {
            auto* m =
                scene::add_model(scene::g_model_files[static_cast<std::size_t>(
                                     scene::g_browser_sel)],
                                 { 0, 0, 6 },
                                 0.1f);
            if (m != nullptr)
            {
                m->animate  = true;
                int new_idx = static_cast<int>(scene::g_models.size()) - 1;
                scene::g_selected = new_idx;
                scene::g_selected_set.clear();
                scene::g_selected_set.insert(new_idx);
                log(2, "Added model to scene: " + sanitize_utf8(m->name));
            }
        }
        if (ImGui::IsItemHovered() && ok)
        {
            ImGui::SetTooltip("Add selected model to the scene at origin");
        }
        if (!ok)
        {
            ImGui::EndDisabled();
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

void draw_audio(euengine::engine_context* ctx)
{
    if (!g_show_audio || (ctx->audio_system == nullptr))
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 300, 420),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(284, 240),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 150),
                                        ImVec2(450, io.DisplaySize.y - 60));

    if (ImGui::Begin("Audio Player", &g_show_audio))
    {
        // Search filter
        ImGui::SetNextItemWidth(-1);
        char filter_buf[256] = {};
        std::strncpy(
            filter_buf, g_audio_filter.c_str(), sizeof(filter_buf) - 1);
        if (ImGui::InputTextWithHint("##audio_filter",
                                     "Search audio...",
                                     filter_buf,
                                     sizeof(filter_buf)))
        {
            g_audio_filter = filter_buf;
        }

        ImGui::Spacing();

        // Track list
        ImGui::BeginChild("##tracks", ImVec2(0, -50), 1);
        for (std::size_t i = 0; i < scene::g_audio.size(); ++i)
        {
            auto& t = scene::g_audio[i];

            // Apply filter
            if (!g_audio_filter.empty() &&
                t.name.find(g_audio_filter) == std::string::npos)
            {
                continue;
            }

            bool playing = std::cmp_equal(i, scene::g_playing);

            ImVec4 col = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
            if (playing)
            {
                col = ImVec4(0.40f, 0.90f, 0.55f, 1.0f);
            }
            else if (t.is_sfx)
            {
                col = ImVec4(0.40f, 0.75f, 0.95f, 1.0f);
            }

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            auto safe_name = sanitize_utf8(t.name);
            if (ImGui::Selectable(safe_name.c_str(), playing))
            {
                if (t.handle == euengine::invalid_music)
                {
                    t.handle = ctx->audio_system->load_music(t.path);
                }
                if (t.handle != euengine::invalid_music)
                {
                    ctx->audio_system->play_music(t.handle, !t.is_sfx);
                    scene::g_playing = static_cast<int>(i);
                    ctx->audio_system->set_music_volume(g_volume / 100.0f);
                }
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        // Controls
        bool playing = ctx->audio_system->is_music_playing();
        bool paused  = ctx->audio_system->is_music_paused();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 6));

        float bw = 60.0f;
        if (playing && !paused)
        {
            if (ImGui::Button("Pause", ImVec2(bw, 28)))
            {
                ctx->audio_system->pause_music();
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.28f, 0.55f, 0.80f, 1.0f));
            if (ImGui::Button("Play", ImVec2(bw, 28)))
            {
                ctx->audio_system->resume_music();
            }
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(bw, 28)))
        {
            ctx->audio_system->stop_music();
            scene::g_playing = -1;
        }
        ImGui::SameLine();

        // Volume
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##vol", &g_volume, 0.0f, 100.0f, "Vol: %.0f%%"))
        {
            ctx->audio_system->set_music_volume(g_volume / 100.0f);
        }

        ImGui::PopStyleVar();
    }
    ImGui::End();
}

void draw_engine(euengine::engine_context* ctx)
{
    if (!g_show_engine)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    // Position above Performance Metrics (which is at bottom-right)
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 420, 40),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, io.DisplaySize.y - 360),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(350, 400),
                                        ImVec2(600, io.DisplaySize.y - 360));

    if (ImGui::Begin("Engine Settings", &g_show_engine))
    {
        // Renderer Info
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Renderer");
        ImGui::Separator();

        ImGui::Text("GPU: %s",
                    sanitize_utf8(std::string(ctx->settings->get_gpu_driver()))
                        .c_str());
        ImGui::Text("Resolution: %d x %d",
                    ctx->settings->get_window_width(),
                    ctx->settings->get_window_height());

        bool fs = ctx->settings->is_fullscreen();
        if (ImGui::Checkbox("Fullscreen (F11)", &fs))
        {
            ctx->settings->set_fullscreen(fs);
        }

        ImGui::Spacing();

        // VSync
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("VSync Mode");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int vs = static_cast<int>(ctx->settings->get_vsync());
        if (ImGui::RadioButton("Enabled", vs == 1))
        {
            ctx->settings->set_vsync(euengine::vsync_mode::enabled);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Adaptive", vs == 2))
        {
            ctx->settings->set_vsync(euengine::vsync_mode::adaptive);
        }

        // If somehow disabled, force to enabled
        if (vs == 0)
        {
            ctx->settings->set_vsync(euengine::vsync_mode::enabled);
        }

        ImGui::Spacing();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Anti-Aliasing Section
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Anti-Aliasing");
        ImGui::Separator();

        // MSAA
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("MSAA");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int msaa = static_cast<int>(ctx->settings->get_msaa());

        // Check GPU support for each MSAA level
        bool msaa2_ok =
            ctx->settings->is_msaa_supported(euengine::msaa_samples::x2);
        bool msaa4_ok =
            ctx->settings->is_msaa_supported(euengine::msaa_samples::x4);
        bool msaa8_ok =
            ctx->settings->is_msaa_supported(euengine::msaa_samples::x8);

        if (ImGui::RadioButton("Off", msaa == 1))
        {
            ctx->settings->set_msaa(euengine::msaa_samples::none);
        }
        ImGui::SameLine();

        ImGui::BeginDisabled(!msaa2_ok);
        if (ImGui::RadioButton("2x", msaa == 2))
        {
            ctx->settings->set_msaa(euengine::msaa_samples::x2);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::BeginDisabled(!msaa4_ok);
        if (ImGui::RadioButton("4x", msaa == 4))
        {
            ctx->settings->set_msaa(euengine::msaa_samples::x4);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::BeginDisabled(!msaa8_ok);
        if (ImGui::RadioButton("8x", msaa == 8))
        {
            ctx->settings->set_msaa(euengine::msaa_samples::x8);
        }
        ImGui::EndDisabled();
        ImGui::Spacing();

        // FXAA (post-processing)
        bool fxaa = ctx->settings->is_fxaa_enabled();
        if (ImGui::Checkbox("FXAA (Post-Process)", &fxaa))
        {
            ctx->settings->set_fxaa_enabled(fxaa);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Fast Approximate Anti-Aliasing (post-processing)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Quality Settings
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Quality");
        ImGui::Separator();

        // Render Scale
        float render_scale = ctx->settings->get_render_scale();
        if (ImGui::SliderFloat(
                "Render Scale", &render_scale, 0.25f, 4.0f, "%.2fx"))
        {
            ctx->settings->set_render_scale(render_scale);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Internal resolution multiplier (0.25x = "
                              "quarter, 1.0x = native, 4.0x = supersampling)");
        }

        // Texture Quality
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("Texture Filtering");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int tex_filter = static_cast<int>(ctx->settings->get_texture_filter());
        if (ImGui::RadioButton("Nearest", tex_filter == 0))
        {
            ctx->settings->set_texture_filter(
                euengine::i_engine_settings::texture_filter::nearest);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Pixelated, fastest - applies to newly loaded textures");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Linear", tex_filter == 1))
        {
            ctx->settings->set_texture_filter(
                euengine::i_engine_settings::texture_filter::linear);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Smooth, good quality - applies to newly loaded textures");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Trilinear", tex_filter == 2))
        {
            ctx->settings->set_texture_filter(
                euengine::i_engine_settings::texture_filter::trilinear);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Best quality with mipmaps - applies to newly loaded textures");
        }

        // Max Anisotropy
        float max_aniso = ctx->settings->get_max_anisotropy();
        if (ImGui::SliderFloat(
                "Anisotropic Filtering", &max_aniso, 1.0f, 16.0f, "%.0fx"))
        {
            ctx->settings->set_max_anisotropy(max_aniso);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Improves texture quality at oblique angles (1x-16x)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Performance Settings
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Performance");
        ImGui::Separator();

        // Frame Buffering
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("Frame Buffering");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int frames = static_cast<int>(ctx->settings->get_frames_in_flight());
        if (ImGui::RadioButton("Single (1)", frames == 1))
        {
            ctx->settings->set_frames_in_flight(1);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Lowest latency, may cause stuttering");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Double (2)", frames == 2))
        {
            ctx->settings->set_frames_in_flight(2);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Balanced latency and smoothness (default)");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Triple (3)", frames == 3))
        {
            ctx->settings->set_frames_in_flight(3);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Smoothest, higher latency");
        }

        // Render Distance
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
        ImGui::Text("Render Distance");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Profiler Frame Marks
        bool frame_marks = ctx->settings->is_profiler_frame_marks_enabled();
        if (ImGui::Checkbox("Profiler Frame Marks", &frame_marks))
        {
            ctx->settings->set_profiler_frame_marks_enabled(frame_marks);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Enable frame marks in Tracy profiler for frame visualization");
        }

        // Profiler Frame Images
        bool frame_images = ctx->settings->is_profiler_frame_images_enabled();
        if (ImGui::Checkbox("Profiler Frame Images", &frame_images))
        {
            ctx->settings->set_profiler_frame_images_enabled(frame_images);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Enable frame screenshots in Tracy (WARNING: Very expensive - "
                "blocks GPU. Captures ~once per second at max 512px "
                "maintaining aspect ratio)");
        }

        ImGui::Spacing();

        float render_dist = ctx->settings->get_render_distance();
        if (ImGui::SliderFloat(
                "##render_dist", &render_dist, 10.0f, 10000.0f, "%.0f units"))
        {
            ctx->settings->set_render_distance(render_dist);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Maximum rendering distance (far plane). Objects "
                              "beyond this distance are not rendered.");
        }

        ImGui::Spacing();

        // Post-Processing settings
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f),
                           "Post-Processing");
        ImGui::Separator();

        float gamma = ctx->settings->get_gamma();
        if (ImGui::SliderFloat("Gamma", &gamma, 1.0f, 3.0f, "%.2f"))
        {
            ctx->settings->set_gamma(gamma);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Display gamma correction (default: 2.2)");
        }

        float brightness = ctx->settings->get_brightness();
        if (ImGui::SliderFloat("Brightness", &brightness, -1.0f, 1.0f, "%.2f"))
        {
            ctx->settings->set_brightness(brightness);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Brightness adjustment (default: 0)");
        }

        float contrast = ctx->settings->get_contrast();
        if (ImGui::SliderFloat("Contrast", &contrast, 0.5f, 2.0f, "%.2f"))
        {
            ctx->settings->set_contrast(contrast);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Contrast adjustment (default: 1.0)");
        }

        float saturation = ctx->settings->get_saturation();
        if (ImGui::SliderFloat("Saturation", &saturation, 0.0f, 2.0f, "%.2f"))
        {
            ctx->settings->set_saturation(saturation);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Color saturation (0.0 = grayscale, 1.0 = "
                              "normal, 2.0 = vibrant)");
        }

        float vignette = ctx->settings->get_vignette();
        if (ImGui::SliderFloat("Vignette", &vignette, 0.0f, 1.0f, "%.2f"))
        {
            ctx->settings->set_vignette(vignette);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Darken screen edges (0.0 = off, 1.0 = maximum)");
        }

        if (ImGui::Button("Reset Post-Processing", ImVec2(-1, 0)))
        {
            ctx->settings->set_gamma(2.2f);
            ctx->settings->set_brightness(0.0f);
            ctx->settings->set_contrast(1.0f);
            ctx->settings->set_saturation(1.0f);
            ctx->settings->set_vignette(0.0f);
        }

        ImGui::Spacing();

        // Shaders
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Shaders");
        ImGui::Separator();

        if (ctx->shader_system != nullptr)
        {
            bool hot = ctx->shader_system->hot_reload_enabled();
            if (ImGui::Checkbox("Enable Hot Reload", &hot))
            {
                ctx->shader_system->enable_hot_reload(hot);
            }

            if (ImGui::Button("Reload Now (F5)", ImVec2(-1, 28)))
            {
                log(2, "Shader hot reload triggered");
                ctx->shader_system->enable_hot_reload(false);
                ctx->shader_system->enable_hot_reload(true);
            }
        }

        ImGui::Spacing();

        // Game Module
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Game Module");
        ImGui::Separator();

        if (!scene::g_lib_path.empty())
        {
            auto name = sanitize_utf8(
                std::filesystem::path(scene::g_lib_path).filename().string());
            ImGui::Text("%s", std::format("File: {}", name).c_str());

            float kb = static_cast<float>(scene::g_lib_size) / 1024.0f;
            float mb = kb / 1024.0f;
            if (mb >= 1.0f)
            {
                ImGui::Text("%s", std::format("Size: {:.2f} MB", mb).c_str());
            }
            else
            {
                ImGui::Text("%s", std::format("Size: {:.1f} KB", kb).c_str());
            }

            // File timestamp
            std::filesystem::path p(scene::g_lib_path);
            if (std::filesystem::exists(p))
            {
                auto ftime = std::filesystem::last_write_time(p);
                auto sctp  = std::chrono::time_point_cast<
                     std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() +
                    std::chrono::system_clock::now());
                auto time_t = std::chrono::system_clock::to_time_t(sctp);
                char buf[64];
                std::strftime(buf,
                              sizeof(buf),
                              "%Y-%m-%d %H:%M:%S",
                              std::localtime(&time_t));
                ImGui::Text("Modified: %s", buf);
            }

            auto safe_path = sanitize_utf8(scene::g_lib_path);
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                               "Path: %s",
                               safe_path.c_str());

            ImGui::Spacing();

            // Hot reload button
            if (ImGui::Button("Hot Reload Module (F5)", ImVec2(-1, 32)))
            {
                log(2, "Game module hot reload triggered");
                if (ctx->settings != nullptr)
                {
                    if (ctx->settings->reload_game())
                    {
                        log(2, "Game module reloaded successfully");
                    }
                    else
                    {
                        log(4, "Failed to reload game module");
                    }
                }
            }
        }
        else
        {
            ImGui::Text("Using built-in game module");
        }
    }
    ImGui::End();
}

void draw_stats(euengine::engine_context* ctx)
{
    [[maybe_unused]] auto profiler_zone =
        profiler_zone_begin(ctx->profiler, "UI::draw_stats");

    if (!g_show_stats)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Enhanced floating overlay - bottom right, larger
    float w = 450.0f;
    float h = 400.0f;
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - w - 16, io.DisplaySize.y - h - 40),
        g_reset_window_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w, h),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);

    // Use title bar like engine settings - allows built-in close button
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));

    // Use built-in close button like engine settings window
    if (ImGui::Begin("Performance Monitor", &g_show_stats, flags))
    {
        ImGui::Separator();

        // Current FPS - large and prominent with color coding
        ImVec4 fps_color;
        if (ctx->time.fps >= 60.0f)
        {
            fps_color = ImVec4(0.40f, 0.80f, 0.50f, 1.0f); // Green for 60+ FPS
        }
        else if (ctx->time.fps >= 30.0f)
        {
            fps_color =
                ImVec4(0.95f, 0.75f, 0.30f, 1.0f); // Yellow for 30-60 FPS
        }
        else
        {
            fps_color = ImVec4(0.95f, 0.40f, 0.40f, 1.0f); // Red for <30 FPS
        }

        ImGui::PushStyleColor(ImGuiCol_Text, fps_color);
        auto fps_text = std::format("{:.1f} FPS", ctx->time.fps);
        ImGui::Text("%s", fps_text.c_str());
        ImGui::PopStyleColor();

        // Frame time next to FPS
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                           "%.2f ms",
                           ctx->time.delta * 1000.0f);

        ImGui::Spacing();

        // Stats grid
        if (ImGui::BeginTable("##stats", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(
                "Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value",
                                    ImGuiTableColumnFlags_WidthStretch);

            // FPS stats
            if (scene::g_max_fps > 0.0f)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                                   "FPS (Min/Avg/Max):");
                ImGui::TableNextColumn();
                ImGui::Text("%.0f / %.0f / %.0f",
                            scene::g_min_fps,
                            scene::g_avg_fps,
                            scene::g_max_fps);
            }

            // Frame count
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "Frame:");
            ImGui::TableNextColumn();
            ImGui::Text("%llu",
                        static_cast<unsigned long long>(ctx->time.frame_count));

            // Elapsed time
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "Elapsed:");
            ImGui::TableNextColumn();
            ImGui::Text("%.1f s", ctx->time.elapsed);

            // Draw calls
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                               "Draw Calls:");
            ImGui::TableNextColumn();
            ImGui::Text("%d", scene::g_draw_calls);

            // Triangles
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "Triangles:");
            ImGui::TableNextColumn();
            ImGui::Text("~%d", scene::g_triangles);

            // Objects
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "Objects:");
            ImGui::TableNextColumn();
            ImGui::Text("%zu", scene::g_models.size());

            // Resolution
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                               "Resolution:");
            ImGui::TableNextColumn();
            ImGui::Text("%d x %d",
                        ctx->settings->get_window_width(),
                        ctx->settings->get_window_height());

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // FPS graph using ImPlot
        constexpr int size = 300;
        int           idx  = scene::g_frame_idx;
        float         reordered_fps[300];

        // Reorder circular buffer for plotting
        for (int i = 0; i < size; ++i)
        {
            int src_idx = (idx + i) % size;
            if (src_idx >= 0 && src_idx < size)
            {
                reordered_fps[i] = scene::g_fps_history[src_idx];
            }
            else
            {
                reordered_fps[i] = 0.0f;
            }
        }

        // ImPlot FPS graph
        // Ensure ImPlot context exists (should be created in ui::init)
        if (ImPlot::GetCurrentContext() == nullptr)
        {
            ImPlot::CreateContext();
        }

        if (ImPlot::BeginPlot("FPS History",
                              ImVec2(-1, 180),
                              ImPlotFlags_NoTitle | ImPlotFlags_NoMenus |
                                  ImPlotFlags_NoBoxSelect |
                                  ImPlotFlags_NoLegend))
        {
            // Setup axes with better formatting
            ImPlot::SetupAxes(
                nullptr,
                "FPS",
                ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels,
                ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, size, ImGuiCond_Always);

            // Auto-scale Y axis based on current FPS range
            float max_fps = 60.0f;
            if (scene::g_max_fps > 0.0f)
            {
                max_fps = std::max(120.0f, scene::g_max_fps * 1.2f);
            }
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, max_fps, ImGuiCond_Always);
            ImPlot::SetupAxisFormat(ImAxis_Y1, "%.0f");

            // Draw FPS line with smooth rendering
            ImPlot::SetNextLineStyle(ImVec4(0.40f, 0.80f, 0.50f, 1.0f), 2.0f);
            ImPlot::PlotLine("FPS", reordered_fps, size);

            // Draw fill under curve with gradient
            ImPlot::SetNextFillStyle(ImVec4(0.40f, 0.80f, 0.50f, 0.15f), 0.0f);
            ImPlot::PlotShaded("##fps_fill", reordered_fps, size);

            ImPlot::EndPlot();
        }

#ifdef TRACY_ENABLE
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                           "Tracy Profiler:");
        ImGui::TextColored(ImVec4(0.40f, 0.70f, 0.95f, 1.0f),
                           "Connect to localhost:8086");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Use Tracy profiler GUI to connect and view "
                              "detailed profiling data");
        }
#endif
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void draw_console()
{
    if (!g_show_console)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(16, io.DisplaySize.y - 280),
                            g_reset_window_layout ? ImGuiCond_Always
                                                  : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 240),
                             g_reset_window_layout ? ImGuiCond_Always
                                                   : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 120),
                                        ImVec2(io.DisplaySize.x - 32, 500));

    if (ImGui::Begin("Console", &g_show_console))
    {
        // Enhanced toolbar
        ImGui::BeginGroup();
        if (ImGui::Button("Clear", ImVec2(60, 0)))
        {
            log_clear();
            log(2, "Console cleared");
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Clear all log entries");
        }
        ImGui::SameLine();

        // Enhanced filter input with clear button
        ImGui::SetNextItemWidth(200);
        char filter_buf[256] = {};
        std::strncpy(
            filter_buf, g_console_filter.c_str(), sizeof(filter_buf) - 1);
        if (ImGui::InputTextWithHint(
                "##filter", "Filter...", filter_buf, sizeof(filter_buf)))
        {
            g_console_filter = filter_buf;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Filter log entries by text");
        }
        if (g_console_filter[0] != '\0')
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
            {
                g_console_filter.clear();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Clear filter");
            }
        }
        ImGui::SameLine();

        std::size_t entry_count =
            g_console_filter.empty()
                ? g_log.size()
                : static_cast<std::size_t>(std::ranges::count_if(
                      g_log,
                      [](const auto& e) {
                          return e.message.find(g_console_filter) !=
                                 std::string::npos;
                      }));
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                           "%s",
                           std::format("{} entries", entry_count).c_str());
        ImGui::EndGroup();

        ImGui::Separator();

        // Enhanced log view with better formatting
        ImGui::BeginChild(
            "##log", ImVec2(0, 0), 1, ImGuiWindowFlags_HorizontalScrollbar);

        bool should_scroll = false;
        for (const auto& e : g_log)
        {
            // Apply filter
            if (!g_console_filter.empty() &&
                e.message.find(g_console_filter) == std::string::npos)
            {
                continue;
            }

            // Timestamp with better formatting
            auto time_str = std::format("[{:.1f}s]", e.time);
            ImGui::TextColored(
                ImVec4(0.45f, 0.45f, 0.48f, 1.0f), "%s", time_str.c_str());
            ImGui::SameLine();

            // Level tag with consistent width
            ImVec4      col;
            const char* tag = nullptr;
            switch (e.level)
            {
                case 0:
                    col = ImVec4(0.50f, 0.50f, 0.53f, 1.0f);
                    tag = "TRACE";
                    break;
                case 1:
                    col = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
                    tag = "DEBUG";
                    break;
                case 2:
                    col = ImVec4(0.38f, 0.68f, 0.93f, 1.0f);
                    tag = "INFO ";
                    break;
                case 3:
                    col = ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
                    tag = "WARN ";
                    break;
                case 4:
                    col = ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
                    tag = "ERROR";
                    break;
                default:
                    col = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
                    tag = "???  ";
                    break;
            }

            ImGui::TextColored(col, "[%s]", tag);
            ImGui::SameLine();
            auto safe_msg = sanitize_utf8(e.message);
            ImGui::TextWrapped("%s", safe_msg.c_str());

            should_scroll = true;
        }

        if (g_log_scroll && should_scroll)
        {
            ImGui::SetScrollHereY(1.0f);
            g_log_scroll = false;
        }

        ImGui::EndChild();
    }
    ImGui::End();
}

void draw_shortcuts()
{
    if (!g_show_shortcuts)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 400),
                                        ImVec2(800, io.DisplaySize.y - 60));

    if (ImGui::Begin("Keyboard Shortcuts", &g_show_shortcuts))
    {
        ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f),
                           "Keyboard Shortcuts");
        ImGui::Separator();
        ImGui::Spacing();

        // Camera Controls
        if (ImGui::CollapsingHeader("Camera Controls",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Columns(2, "shortcuts", false);
            ImGui::SetColumnWidth(0, 200);

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "W/A/S/D");
            ImGui::NextColumn();
            ImGui::Text("Move camera forward/left/backward/right");
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Q/E");
            ImGui::NextColumn();
            ImGui::Text("Move camera down/up");
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Shift");
            ImGui::NextColumn();
            ImGui::Text("Hold to move faster");
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "ESC");
            ImGui::NextColumn();
            ImGui::Text("Release mouse capture");
            ImGui::NextColumn();

            ImGui::Columns(1);
        }

        // View Controls
        if (ImGui::CollapsingHeader("View Controls"))
        {
            ImGui::Columns(2, "shortcuts2", false);
            ImGui::SetColumnWidth(0, 200);

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Tab");
            ImGui::NextColumn();
            ImGui::Text("Toggle wireframe mode");
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Space");
            ImGui::NextColumn();
            ImGui::Text("Toggle auto animation");
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "F11");
            ImGui::NextColumn();
            ImGui::Text("Toggle fullscreen");
            ImGui::NextColumn();

            ImGui::Columns(1);
        }

        // File Operations
        if (ImGui::CollapsingHeader("File Operations"))
        {
            ImGui::Columns(2, "shortcuts3", false);
            ImGui::SetColumnWidth(0, 200);

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "F5");
            ImGui::NextColumn();
            ImGui::Text("Hot reload shaders and game module");
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Ctrl+O");
            ImGui::NextColumn();
            ImGui::Text("Open scene file dialog");
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "Ctrl+N");
            ImGui::NextColumn();
            ImGui::Text("Clear scene");
            ImGui::NextColumn();

            ImGui::Columns(1);
        }

        // UI Controls
        if (ImGui::CollapsingHeader("UI Controls"))
        {
            ImGui::Columns(2, "shortcuts4", false);
            ImGui::SetColumnWidth(0, 200);

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "` (Grave)");
            ImGui::NextColumn();
            ImGui::Text("Toggle console");
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f), "F1");
            ImGui::NextColumn();
            ImGui::Text("Show this shortcuts window");
            ImGui::NextColumn();

            ImGui::Columns(1);
        }

        // Object Interaction
        if (ImGui::CollapsingHeader("Object Interaction"))
        {
            ImGui::Columns(2, "shortcuts5", false);
            ImGui::SetColumnWidth(0, 200);

            ImGui::TextColored(ImVec4(0.38f, 0.68f, 0.93f, 1.0f),
                               "Double-Click");
            ImGui::NextColumn();
            ImGui::Text("Focus camera on selected object");
            ImGui::NextColumn();

            ImGui::Columns(1);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f),
                           "Tip: Click in the 3D viewport to capture mouse and "
                           "control the camera");
    }
    ImGui::End();
}

void draw_statusbar(euengine::engine_context* ctx)
{
    ImGuiIO& io = ImGui::GetIO();

    float h = 28.0f;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - h));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, h));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0.067f, 0.067f, 0.075f, 1.0f));

    if (ImGui::Begin("##bar", nullptr, flags))
    {
        const char* help =
            ctx->input.mouse_captured
                ? "WASD Move | QE Up/Down | Shift Speed | Space Animate | Tab "
                  "Wireframe | F5 Reload | ESC Release | F1 Help"
                : "Click to capture mouse | F5 Reload | F11 Fullscreen | ` "
                  "Console | F1 Shortcuts";

        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.0f), "%s", help);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace

void log(int level, const std::string& msg)
{
    g_log.push_back({ msg, level, g_time });
    if (g_log.size() > g_log_max)
    {
        g_log.pop_front();
    }
    g_log_scroll = true;
}

void log_clear()
{
    g_log.clear();
}

void init()
{
    apply_modern_theme();

    // Initialize ImPlot context - it will automatically link to the current
    // ImGui context Make sure ImGui context is set before calling this (should
    // be done in game_init)
    ImPlotContext* plot_ctx = ImPlot::CreateContext();
    if (plot_ctx == nullptr)
    {
        spdlog::error("Failed to create ImPlot context");
    }
    // CreateContext automatically sets it as current, so we don't need
    // SetCurrentContext
}

void draw(euengine::engine_context* ctx)
{
    ImGuiIO& io = ImGui::GetIO();

    // When camera is focused (mouse captured), disable UI mouse interaction
    // This prevents UI from interfering with camera control
    if (ctx->input.mouse_captured)
    {
        io.WantCaptureMouse = false;
    }

    draw_menu(ctx);
    draw_scene(ctx);
    draw_inspector();
    draw_browser();
    draw_audio(ctx);
    draw_engine(ctx);
    draw_stats(ctx);
    draw_console();
    draw_file_dialog();
    draw_shortcuts();
    draw_statusbar(ctx);

    // Reset window layout flag after all windows have been drawn
    if (g_reset_window_layout)
    {
        g_reset_window_layout = false;
    }

    // Keep UI from capturing mouse when camera is focused
    if (ctx->input.mouse_captured)
    {
        io.WantCaptureMouse = false;
    }

    // Display key sequence in bottom left corner (vim-like) - draw last so it's
    // on top
    if (ctx->key_sequence != nullptr && std::strlen(ctx->key_sequence) > 0)
    {
        // Position with proper padding from edges to ensure full visibility
        const float padding = 15.0f;
        ImGui::SetNextWindowPos(
            ImVec2(padding, io.DisplaySize.y - 35.0f - padding),
            ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.85f); // Semi-transparent
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing;

        // Set window padding using style var
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
        if (ImGui::Begin("KeySequence", nullptr, flags))
        {
            ImGui::TextColored(
                ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", ctx->key_sequence);
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

void draw_file_dialog()
{
    if (!g_show_file_dialog)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_Appearing);

    if (ImGui::Begin("Load glTF/GLB Scene",
                     &g_show_file_dialog,
                     ImGuiWindowFlags_NoCollapse))
    {
        // Header with file type info
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
        ImGui::TextWrapped(
            "Select a glTF (.gltf) or GLB (.glb) scene file to load");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Current path display
        ImGui::Text("Current Directory:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
        ImGui::TextWrapped(
            "%s", sanitize_utf8(g_file_dialog_current_path.string()).c_str());
        ImGui::PopStyleColor();

        ImGui::Separator();

        // Navigation buttons with better styling
        ImGui::Spacing();
        if (ImGui::Button("^ Up", ImVec2(80, 0)))
        {
            if (g_file_dialog_current_path.has_parent_path())
            {
                g_file_dialog_current_path =
                    g_file_dialog_current_path.parent_path();
                g_file_dialog_selected_file.clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Home", ImVec2(80, 0)))
        {
            g_file_dialog_current_path = std::filesystem::path(
                (std::getenv("HOME") != nullptr) ? std::getenv("HOME") : ".");
            g_file_dialog_selected_file.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Current", ImVec2(90, 0)))
        {
            g_file_dialog_current_path = std::filesystem::current_path();
            g_file_dialog_selected_file.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Assets", ImVec2(90, 0)))
        {
            g_file_dialog_current_path = std::filesystem::path("assets");
            g_file_dialog_selected_file.clear();
        }

        ImGui::Separator();

        // File list
        ImGui::BeginChild("##file_list",
                          ImVec2(0, -80),
                          1,
                          ImGuiWindowFlags_HorizontalScrollbar);

        try
        {
            // Show parent directory
            if (g_file_dialog_current_path.has_parent_path())
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.7f, 0.7f, 1.0f, 1.0f));
                bool is_selected = false;
                if (ImGui::Selectable("[..]", is_selected, 0, ImVec2(0, 22)))
                {
                    g_file_dialog_current_path =
                        g_file_dialog_current_path.parent_path();
                    g_file_dialog_selected_file.clear();
                }
                ImGui::PopStyleColor();
            }

            // List directories and glTF/GLB files (Godot export format)
            std::vector<std::filesystem::path> entries;
            for (const auto& entry : std::filesystem::directory_iterator(
                     g_file_dialog_current_path))
            {
                entries.push_back(entry.path());
            }
            std::sort(entries.begin(),
                      entries.end(),
                      [](const auto& a, const auto& b)
                      {
                          bool a_is_dir = std::filesystem::is_directory(a);
                          bool b_is_dir = std::filesystem::is_directory(b);
                          if (a_is_dir != b_is_dir)
                          {
                              return a_is_dir > b_is_dir; // Directories first
                          }
                          return a.filename().string() < b.filename().string();
                      });

            for (const auto& entry : entries)
            {
                bool is_dir = std::filesystem::is_directory(entry);
                auto ext    = entry.extension().string();
                std::ranges::transform(ext, ext.begin(), ::tolower);
                bool is_gltf = !is_dir && (ext == ".gltf" || ext == ".glb");

                if (!is_dir && !is_gltf)
                {
                    continue; // Skip non-glTF files
                }

                std::string name = sanitize_utf8(entry.filename().string());
                // Compare normalized paths for selection
                bool is_selected = false;
                if (!g_file_dialog_selected_file.empty())
                {
                    try
                    {
                        auto normalized_entry =
                            std::filesystem::canonical(entry);
                        is_selected =
                            (g_file_dialog_selected_file == normalized_entry);
                    }
                    catch (...)
                    {
                        try
                        {
                            auto abs_entry = std::filesystem::absolute(entry)
                                                 .lexically_normal();
                            auto abs_selected = std::filesystem::absolute(
                                                    g_file_dialog_selected_file)
                                                    .lexically_normal();
                            is_selected = (abs_selected == abs_entry);
                        }
                        catch (...)
                        {
                            is_selected =
                                (g_file_dialog_selected_file == entry);
                        }
                    }
                }

                if (is_dir)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
                    std::string dir_label = "[DIR] " + name;
                    if (ImGui::Selectable(dir_label.c_str(),
                                          false,
                                          ImGuiSelectableFlags_AllowDoubleClick,
                                          ImVec2(0, 24)))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            g_file_dialog_current_path = entry;
                            g_file_dialog_selected_file.clear();
                        }
                    }
                    ImGui::PopStyleColor();
                }
                else if (is_gltf)
                {
                    // Different colors for .gltf vs .glb
                    ImVec4 file_color =
                        (ext == ".glb")
                            ? ImVec4(0.6f, 0.9f, 1.0f, 1.0f)  // Cyan for GLB
                            : ImVec4(1.0f, 0.9f, 0.6f, 1.0f); // Yellow for glTF

                    // Show file type prefix
                    std::string file_label =
                        (ext == ".glb") ? "[GLB] " : "[glTF] ";
                    file_label += name;

                    // Highlight selected file
                    if (is_selected)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Header,
                                              ImVec4(0.3f, 0.5f, 0.7f, 0.5f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                              ImVec4(0.4f, 0.6f, 0.8f, 0.7f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                              ImVec4(0.5f, 0.7f, 0.9f, 0.9f));
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, file_color);
                    if (ImGui::Selectable(file_label.c_str(),
                                          is_selected,
                                          ImGuiSelectableFlags_AllowDoubleClick,
                                          ImVec2(0, 24)))
                    {
                        // Always update selection on click - normalize path
                        try
                        {
                            g_file_dialog_selected_file =
                                std::filesystem::canonical(entry);
                            spdlog::debug("Selected file (canonical): {}",
                                          g_file_dialog_selected_file.string());
                        }
                        catch (const std::exception&)
                        {
                            try
                            {
                                g_file_dialog_selected_file =
                                    std::filesystem::absolute(entry)
                                        .lexically_normal();
                                spdlog::debug(
                                    "Selected file (absolute): {}",
                                    g_file_dialog_selected_file.string());
                            }
                            catch (const std::exception& e2)
                            {
                                g_file_dialog_selected_file = entry;
                                spdlog::warn("Could not normalize path, using "
                                             "raw: {}, error: {}",
                                             entry.string(),
                                             e2.what());
                            }
                        }
                        log(2,
                            "Selected file: " +
                                sanitize_utf8(
                                    g_file_dialog_selected_file.string()));

                        // Double-click to load immediately
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            load_gltf_glb_scene(g_file_dialog_selected_file);
                        }
                    }
                    ImGui::PopStyleColor();

                    if (is_selected)
                    {
                        ImGui::PopStyleColor(3);
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            std::string error_msg = sanitize_utf8(e.what());
            ImGui::TextColored(
                ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", error_msg.c_str());
        }

        ImGui::EndChild();

        ImGui::Separator();

        // Selected file display with better formatting
        ImGui::Spacing();
        ImGui::Text("Selected File:");
        ImGui::SameLine();
        if (!g_file_dialog_selected_file.empty())
        {
            auto ext = g_file_dialog_selected_file.extension().string();
            std::ranges::transform(ext, ext.begin(), ::tolower);
            std::string file_type = (ext == ".glb") ? "GLB" : "glTF";

            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
            std::string filename =
                g_file_dialog_selected_file.filename().string();
            ImGui::Text("%s", sanitize_utf8(filename).c_str());
            ImGui::PopStyleColor();

            // Show file type and size
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::Text("(%s)", file_type.c_str());
            ImGui::PopStyleColor();

            // Try to show file size
            try
            {
                if (std::filesystem::exists(g_file_dialog_selected_file))
                {
                    auto file_size =
                        std::filesystem::file_size(g_file_dialog_selected_file);
                    std::string size_str;
                    if (file_size < 1024)
                    {
                        size_str = std::to_string(file_size) + " B";
                    }
                    else if (file_size < 1024 * 1024)
                    {
                        size_str = std::to_string(file_size / 1024) + " KB";
                    }
                    else
                    {
                        size_str =
                            std::to_string(file_size / (1024 * 1024)) + " MB";
                    }

                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    ImGui::Text(" - %s", size_str.c_str());
                    ImGui::PopStyleColor();
                }
            }
            catch (...)
            {
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               "No file selected");
        }

        ImGui::Spacing();

        // Action buttons with better styling
        bool has_selection = false;
        if (!g_file_dialog_selected_file.empty())
        {
            try
            {
                has_selection =
                    std::filesystem::exists(g_file_dialog_selected_file) &&
                    std::filesystem::is_regular_file(
                        g_file_dialog_selected_file);
            }
            catch (...)
            {
                has_selection = false;
            }
        }

        // Error message
        if (!g_file_dialog_selected_file.empty() && !has_selection)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::Text("! File not found or invalid");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Button layout
        float button_width    = 120.0f;
        float spacing         = ImGui::GetStyle().ItemSpacing.x;
        float available_width = ImGui::GetContentRegionAvail().x;
        float buttons_width   = (button_width * 2) + spacing;
        float offset          = (available_width - buttons_width) * 0.5f;

        if (offset > 0)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        }

        ImGui::BeginDisabled(!has_selection);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
        if (ImGui::Button("Load Scene", ImVec2(button_width, 35)))
        {
            if (has_selection && !g_file_dialog_selected_file.empty())
            {
                load_gltf_glb_scene(g_file_dialog_selected_file);
            }
            else
            {
                log(4, "Load button clicked but no valid selection");
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(button_width, 35)))
        {
            g_show_file_dialog = false;
            g_file_dialog_selected_file.clear();
        }

        // Help text
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextWrapped("Tip: Double-click a file to load it immediately");
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

} // namespace ui

#pragma GCC diagnostic pop