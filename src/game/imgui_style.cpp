#include "imgui_style.hpp"

#include <imgui.h>

namespace ui
{

void apply_modern_theme()
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // Modern dark palette - inspired by Steam, Fluent Design, and JetBrains
    // IDEs Deep background layers for depth
    const ImVec4 bg_deepest =
        ImVec4(0.06f, 0.06f, 0.07f, 1.0f); // #0F0F11 - Deepest layer
    const ImVec4 bg_deep =
        ImVec4(0.08f, 0.08f, 0.09f, 1.0f); // #141416 - Deep layer
    const ImVec4 bg_main =
        ImVec4(0.10f, 0.10f, 0.12f, 1.0f); // #1A1A1E - Main background
    const ImVec4 bg_panel =
        ImVec4(0.13f, 0.13f, 0.15f, 1.0f); // #212124 - Panel background
    const ImVec4 bg_hover =
        ImVec4(0.18f, 0.18f, 0.20f, 1.0f); // #2E2E33 - Hover state
    const ImVec4 bg_active =
        ImVec4(0.24f, 0.24f, 0.27f, 1.0f); // #3D3D45 - Active state

    // Modern accent colors - vibrant blue-cyan gradient
    const ImVec4 accent =
        ImVec4(0.40f, 0.70f, 0.95f, 1.0f); // #66B3F2 - Primary accent
    const ImVec4 accent_hover =
        ImVec4(0.50f, 0.78f, 1.00f, 1.0f); // #7FC7FF - Hover accent
    const ImVec4 accent_active =
        ImVec4(0.30f, 0.60f, 0.85f, 1.0f); // #4D99D9 - Active accent
    const ImVec4 accent_dim =
        ImVec4(0.25f, 0.50f, 0.70f, 1.0f); // #407FB3 - Dimmed accent

    // Text colors - high contrast for readability
    const ImVec4 text_primary =
        ImVec4(0.96f, 0.96f, 0.97f, 1.0f); // #F5F5F7 - Primary text
    const ImVec4 text_disabled =
        ImVec4(0.50f, 0.50f, 0.53f, 1.0f); // #808086 - Disabled text

    // Border colors
    const ImVec4 border = ImVec4(0.22f, 0.22f, 0.25f, 0.60f); // Subtle borders
    const ImVec4 border_strong =
        ImVec4(0.30f, 0.30f, 0.35f, 0.80f); // Strong borders

    // === Core Colors ===
    colors[ImGuiCol_Text]         = text_primary;
    colors[ImGuiCol_TextDisabled] = text_disabled;
    colors[ImGuiCol_WindowBg]     = bg_main;
    colors[ImGuiCol_ChildBg]      = bg_deep;
    colors[ImGuiCol_PopupBg] =
        ImVec4(bg_panel.x, bg_panel.y, bg_panel.z, 0.98f);
    colors[ImGuiCol_Border]       = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // === Frame Colors ===
    colors[ImGuiCol_FrameBg]        = bg_panel;
    colors[ImGuiCol_FrameBgHovered] = bg_hover;
    colors[ImGuiCol_FrameBgActive]  = bg_active;

    // === Title Bar ===
    colors[ImGuiCol_TitleBg]          = bg_deepest;
    colors[ImGuiCol_TitleBgActive]    = bg_panel;
    colors[ImGuiCol_TitleBgCollapsed] = bg_deepest;

    // === Menu Bar ===
    colors[ImGuiCol_MenuBarBg] = bg_deepest;

    // === Scrollbar ===
    colors[ImGuiCol_ScrollbarBg]          = bg_deep;
    colors[ImGuiCol_ScrollbarGrab]        = bg_hover;
    colors[ImGuiCol_ScrollbarGrabHovered] = accent_dim;
    colors[ImGuiCol_ScrollbarGrabActive]  = accent;

    // === Checkbox & Slider ===
    colors[ImGuiCol_CheckMark]        = accent;
    colors[ImGuiCol_SliderGrab]       = accent_dim;
    colors[ImGuiCol_SliderGrabActive] = accent;

    // === Buttons ===
    colors[ImGuiCol_Button]        = bg_hover;
    colors[ImGuiCol_ButtonHovered] = accent_dim;
    colors[ImGuiCol_ButtonActive]  = accent_active;

    // === Headers (for trees, tables, etc.) ===
    colors[ImGuiCol_Header]        = bg_hover;
    colors[ImGuiCol_HeaderHovered] = accent_dim;
    colors[ImGuiCol_HeaderActive]  = accent_active;

    // === Separators ===
    colors[ImGuiCol_Separator]        = border;
    colors[ImGuiCol_SeparatorHovered] = accent_dim;
    colors[ImGuiCol_SeparatorActive]  = accent;

    // === Resize Grips ===
    colors[ImGuiCol_ResizeGrip]        = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ResizeGripHovered] = accent_dim;
    colors[ImGuiCol_ResizeGripActive]  = accent;

    // === Tabs ===
    colors[ImGuiCol_Tab]                = bg_panel;
    colors[ImGuiCol_TabHovered]         = accent_dim;
    colors[ImGuiCol_TabActive]          = accent;
    colors[ImGuiCol_TabUnfocused]       = bg_main;
    colors[ImGuiCol_TabUnfocusedActive] = bg_hover;

    // === Plot Lines/Histograms ===
    colors[ImGuiCol_PlotLines]            = accent;
    colors[ImGuiCol_PlotLinesHovered]     = accent_hover;
    colors[ImGuiCol_PlotHistogram]        = accent_dim;
    colors[ImGuiCol_PlotHistogramHovered] = accent;

    // === Tables ===
    colors[ImGuiCol_TableHeaderBg]     = bg_panel;
    colors[ImGuiCol_TableBorderStrong] = border_strong;
    colors[ImGuiCol_TableBorderLight]  = border;
    colors[ImGuiCol_TableRowBg]        = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]     = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);

    // === Selection & Drag Drop ===
    colors[ImGuiCol_TextSelectedBg] =
        ImVec4(accent.x, accent.y, accent.z, 0.35f);
    colors[ImGuiCol_DragDropTarget] = accent_hover;

    // === Navigation ===
    colors[ImGuiCol_NavHighlight]          = accent;
    colors[ImGuiCol_NavWindowingHighlight] = text_primary;
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.05f, 0.05f, 0.06f, 0.25f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.03f, 0.03f, 0.04f, 0.80f);

    // === Modern Style Metrics ===
    // Rounded corners - modern and friendly
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 5.0f;

    // === Spacing & Padding ===
    // Generous spacing for a clean, uncluttered look
    style.WindowPadding     = ImVec2(16.0f, 16.0f);
    style.FramePadding      = ImVec2(10.0f, 6.0f);
    style.ItemSpacing       = ImVec2(12.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(10.0f, 6.0f);
    style.IndentSpacing     = 24.0f;
    style.ScrollbarSize     = 18.0f;
    style.GrabMinSize       = 16.0f;
    style.CellPadding       = ImVec2(8.0f, 6.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);

    // === Border Sizes ===
    // Minimal borders for a clean look
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 0.0f;
    style.PopupBorderSize  = 1.0f;
    style.FrameBorderSize  = 0.0f;
    style.TabBorderSize    = 0.0f;

    // === Alignment & Positioning ===
    style.WindowTitleAlign         = ImVec2(0.0f, 0.5f); // Left-aligned titles
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition      = ImGuiDir_Right;
    style.ButtonTextAlign          = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign      = ImVec2(0.0f, 0.0f);
    style.DisplaySafeAreaPadding   = ImVec2(4.0f, 4.0f);

    // === Alpha ===
    style.Alpha         = 1.0f;
    style.DisabledAlpha = 0.60f;
}

} // namespace ui
