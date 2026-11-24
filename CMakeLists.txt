cmake_minimum_required(VERSION 4.5)
project(occt_lab LANGUAGES C CXX)

find_package(OpenCASCADE 8.0.1 CONFIG REQUIRED COMPONENTS
    FoundationClasses ModelingData ModelingAlgorithms Visualization ApplicationFramework DataExchange)
find_package(SDL3 3.4.20 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Vulkan 1.4.363 REQUIRED)
find_package(slang 2026.22 CONFIG REQUIRED)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_library(occt_lab_geometry STATIC src/geometry/curves/curve_fitting.cpp)
target_sources(occt_lab_geometry PUBLIC
    FILE_SET CXX_MODULES FILES
        src/geometry/brep/box_model.cppm
        src/geometry/brep/brep_face_properties.cppm
        src/geometry/brep/brep_mesh.cppm
        src/geometry/brep/plate_with_hole.cppm
        src/geometry/bspline_knot.cppm
        src/geometry/bspline_weights.cppm
        src/geometry/curves/bspline_curve.cppm
        src/geometry/occt_bspline_knots.cppm
        src/geometry/surfaces/bernstein_reference.cppm
        src/geometry/surfaces/bezier_patch.cppm
        src/geometry/surfaces/bspline_surface.cppm
        src/geometry/surfaces/occt_bezier_patch.cppm
        src/geometry/surfaces/patch_grid.cppm
        src/geometry/surfaces/surface_isolines.cppm
        src/geometry/vec3.cppm
)
target_compile_features(occt_lab_geometry PUBLIC cxx_std_23)
target_link_libraries(occt_lab_geometry
    PUBLIC TKBRep TKG3d
    PRIVATE TKMesh TKPrim TKBO TKTopAlgo TKGeomAlgo TKMath TKernel
)

add_library(occt_lab_model STATIC
    src/application/surface_approximation.cpp
    src/application/surface_interpolation.cpp
)
target_link_libraries(occt_lab_model PUBLIC occt_lab_geometry PRIVATE TKDESTEP TKXSBase TKMath)
target_sources(occt_lab_model PUBLIC
    FILE_SET CXX_MODULES FILES
        src/application/bezier_patch_presets.cppm
        src/application/editable_curve.cppm
        src/application/editable_nurbs_surface.cppm
        src/application/nurbs_curve_creation.cppm
        src/application/nurbs_surface_creation.cppm
        src/application/single_step_history.cppm
        src/document/curve_project.cppm
        src/document/nurbs_surface_project.cppm
        src/document/plate_step.cppm
        src/document/project_io.cppm
        src/render/compute/nurbs_grid_input.cppm
        src/render/compute/nurbs_grid_report.cppm
        src/render/viewport/brep_face_picking.cppm
        src/render/viewport/clip_transform.cppm
)

add_executable(occt_lab
    src/main.cpp
    src/ui/curve/curve_creation_dialog.cpp
    src/ui/curve/curve_point_picker.cpp
    src/ui/curve/curve_project_panel.cpp
    src/ui/surface/nurbs_surface_creation_dialog.cpp
    src/ui/surface/nurbs_surface_probe.cpp
    src/ui/surface/nurbs_surface_project_panel.cpp
    src/ui/surface/surface_control_net_input.cpp
)
target_sources(occt_lab PRIVATE
    FILE_SET CXX_MODULES FILES
        src/render/compute/compute_buffer_check.cppm
        src/render/compute/compute_pyramid.cppm
        src/render/compute/nurbs_grid_mesh.cppm
        src/render/compute/storage_buffer_kernel.cppm
        src/render/preview/brep_preview.cppm
        src/render/preview/curve_colors.cppm
        src/render/preview/curve_preview.cppm
        src/render/preview/surface/nurbs_surface_preview.cppm
        src/render/preview/surface/surface_probe_marker.cppm
        src/render/preview/surface/surface_wireframe.cppm
        src/render/viewport/orbit_camera.cppm
        src/render/viewport/viewport_projection.cppm
        src/render/vulkan_resources.cppm
        src/ui/box_editor.cppm
        src/ui/clamped_knot_editor.cppm
        src/ui/curve/curve_editor.cppm
        src/ui/curve/fit_residuals.cppm
        src/ui/developer_diagnostics.cppm
        src/ui/imgui_session.cppm
        src/ui/plate_step_panel.cppm
        src/ui/pole_drag.cppm
        src/ui/project_file_controls.cppm
        src/ui/renderer_status_panel.cppm
        src/ui/surface/isoline_sampling.cppm
        src/ui/surface/nurbs_pole_inspector.cppm
        src/ui/surface/nurbs_surface_session.cppm
        src/ui/surface/surface_grid_controls.cppm
        src/ui/surface/surface_sample_readout.cppm
        src/vulkan_window.cppm
)
target_sources(occt_lab PRIVATE
    FILE_SET vulkan_hpp TYPE CXX_MODULES
    BASE_DIRS "${Vulkan_INCLUDE_DIR}"
    FILES "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
)
target_link_libraries(occt_lab PRIVATE occt_lab_model TKPrim TKMath TKDESTEP TKXSBase SDL3::SDL3 imgui::imgui Vulkan::Vulkan)
target_compile_definitions(occt_lab PRIVATE VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0)

set(triangle_shader "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.slang")
set(shader_output_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(triangle_spv "${shader_output_dir}/triangle.vert.spv")

add_custom_command(
    OUTPUT "${triangle_spv}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_output_dir}"
    COMMAND "${SLANG_EXECUTABLE}" "${triangle_shader}"
        -entry vertexMain -target spirv -o "${triangle_spv}"
    DEPENDS "${triangle_shader}" "${SLANG_EXECUTABLE}"
    COMMENT "Compiling triangle.slang to SPIR-V"
    VERBATIM
)

set(preview_shader "${CMAKE_CURRENT_SOURCE_DIR}/shaders/brep_preview.slang")
set(preview_vertex_spv "${shader_output_dir}/brep_preview.vert.spv")
set(preview_fragment_spv "${shader_output_dir}/brep_preview.frag.spv")
add_custom_command(
    OUTPUT "${preview_vertex_spv}" "${preview_fragment_spv}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_output_dir}"
    COMMAND "${SLANG_EXECUTABLE}" "${preview_shader}"
        -entry vertexMain -target spirv -fvk-use-entrypoint-name -o "${preview_vertex_spv}"
    COMMAND "${SLANG_EXECUTABLE}" "${preview_shader}"
        -entry fragmentMain -target spirv -fvk-use-entrypoint-name -o "${preview_fragment_spv}"
    DEPENDS "${preview_shader}" "${SLANG_EXECUTABLE}"
    COMMENT "Compiling B-Rep preview shaders to SPIR-V"
    VERBATIM
)

foreach(name IN ITEMS buffer_check compute_pyramid nurbs_grid)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}.slang")
    set(output "${shader_output_dir}/${name}.comp.spv")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_output_dir}"
        COMMAND "${SLANG_EXECUTABLE}" "${source}"
            -entry computeMain -stage compute -target spirv -fvk-use-entrypoint-name -o "${output}"
        DEPENDS "${source}" "${SLANG_EXECUTABLE}"
        COMMENT "Compiling ${name}.slang to SPIR-V"
        VERBATIM
    )
    list(APPEND compute_shaders "${output}")
endforeach()

add_custom_target(occt_lab_shaders DEPENDS
    "${triangle_spv}" "${preview_vertex_spv}" "${preview_fragment_spv}" ${compute_shaders}
)
add_dependencies(occt_lab occt_lab_shaders)

include(CTest)
if(BUILD_TESTING)
    add_library(occt_lab_test_support STATIC)
    target_link_libraries(occt_lab_test_support PUBLIC occt_lab_geometry)
    target_sources(occt_lab_test_support PUBLIC
        FILE_SET CXX_MODULES FILES
            tests/support/near.cppm
            tests/support/project_test_directory.cppm
    )

    foreach(check IN ITEMS occt_box_check occt_cylinder_check occt_plate_with_hole_check bezier_patch_check bspline_curve_check bspline_surface_check)
        add_executable(${check} tests/${check}/main.cpp)
        add_test(NAME ${check} COMMAND ${check})
    endforeach()

    target_link_libraries(occt_box_check PRIVATE occt_lab_geometry TKTopAlgo TKGeomBase)
    target_link_libraries(occt_cylinder_check PRIVATE occt_lab_model TKPrim)
    target_link_libraries(occt_plate_with_hole_check PRIVATE occt_lab_model occt_lab_test_support TKDESTEP TKXSBase)
    target_link_libraries(bezier_patch_check PRIVATE occt_lab_test_support)
    target_link_libraries(bspline_surface_check PRIVATE occt_lab_model occt_lab_test_support TKMath)
    target_link_libraries(bspline_curve_check PRIVATE occt_lab_model occt_lab_test_support)

    target_sources(bspline_surface_check PRIVATE
        tests/bspline_surface_check/control_net_creation_checks.cpp
        tests/bspline_surface_check/custom_knot_checks.cpp
        tests/bspline_surface_check/degree_elevation_checks.cpp
        tests/bspline_surface_check/evaluation_checks.cpp
        tests/bspline_surface_check/gpu_input_checks.cpp
        tests/bspline_surface_check/grid_checks.cpp
        tests/bspline_surface_check/knot_insertion_checks.cpp
        tests/bspline_surface_check/pole_edit_checks.cpp
        tests/bspline_surface_check/point_grid_approximation_checks.cpp
        tests/bspline_surface_check/point_grid_interpolation_checks.cpp
        tests/bspline_surface_check/preset_checks.cpp
        tests/bspline_surface_check/project_checks.cpp
        tests/bspline_surface_check/sampling_checks.cpp
        PRIVATE FILE_SET CXX_MODULES FILES tests/bspline_surface_check/analytic_surface.cppm
    )
    target_sources(bspline_curve_check PRIVATE
        tests/bspline_curve_check/creation_checks.cpp
        tests/bspline_curve_check/curve_project_checks.cpp
        tests/bspline_curve_check/editing_checks.cpp
        tests/bspline_curve_check/fitting_checks.cpp
        tests/bspline_curve_check/refinement_checks.cpp
        tests/bspline_curve_check/viewport_picking_checks.cpp
    )
endif()
