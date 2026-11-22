#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class OrbitCamera;
enum class ProjectionMode;
struct PatchLayers;

enum class PreviewMode
{
    box,
    cylinder,
    bspline,
    nurbs_surface,
    compute_pyramid,
};

struct PyramidControls
{
    int height_mm = 50;
    std::string error;
};

struct RendererStatus
{
    const char *device_name;
    std::uint32_t api_version;
    std::uint32_t graphics_family;
    std::uint32_t present_family;
    bool compute_supported;
    std::uint32_t width;
    std::uint32_t height;
    std::size_t image_count;
};

void draw_status_panel(const RendererStatus &status, PreviewMode &mode, ProjectionMode &projection, OrbitCamera &camera,
                       bool moving_pole, PatchLayers &patch_layers, bool &show_diagnostics, PyramidControls &pyramid);
