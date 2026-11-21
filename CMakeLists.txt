cmake_minimum_required(VERSION 3.21)
project(occt_lab LANGUAGES C CXX)

find_package(OpenCASCADE CONFIG REQUIRED COMPONENTS FoundationClasses ModelingData ModelingAlgorithms)
find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(slang CONFIG REQUIRED)

set(CMAKE_CXX_EXTENSIONS OFF)

add_library(occt_lab_geometry STATIC
    src/geometry/bspline_knot.hpp
    src/geometry/bspline_weights.hpp
    src/geometry/occt_bspline_knots.cpp
    src/geometry/occt_bspline_knots.hpp
    src/geometry/brep/box_model.cpp
    src/geometry/brep/box_model.hpp
    src/geometry/brep/brep_mesh.cpp
    src/geometry/brep/brep_mesh.hpp
    src/geometry/curves/bspline_curve.cpp
    src/geometry/curves/bspline_curve.hpp
    src/geometry/curves/curve_fitting.cpp
    src/geometry/curves/occt_bspline.hpp
    src/geometry/surfaces/bezier_patch.cpp
    src/geometry/surfaces/bezier_patch.hpp
    src/geometry/surfaces/bspline_surface.cpp
    src/geometry/surfaces/bspline_surface.hpp
    src/geometry/surfaces/occt_bezier_patch.cpp
    src/geometry/surfaces/occt_bezier_patch.hpp
    src/geometry/vec3.hpp
)
target_compile_features(occt_lab_geometry PUBLIC cxx_std_23)
target_include_directories(occt_lab_geometry PUBLIC src)
target_link_libraries(occt_lab_geometry
    PUBLIC TKBRep TKG3d
    PRIVATE TKMesh TKPrim TKTopAlgo TKGeomAlgo TKMath TKernel
)

add_executable(occt_lab
    src/main.cpp
    src/application/editable_curve.cpp
    src/application/editable_curve.hpp
    src/application/editable_patch.cpp
    src/application/editable_patch.hpp
    src/document/curve_project.cpp
    src/document/curve_project.hpp
    src/document/patch_project.cpp
    src/document/patch_project.hpp
    src/document/project_io.cpp
    src/document/project_io.hpp
    src/vulkan_window.cpp
    src/vulkan_window.hpp
    src/render/compute/compute_bezier_patch.cpp
    src/render/compute/compute_bezier_patch.hpp
    src/render/compute/compute_buffer_check.cpp
    src/render/compute/compute_buffer_check.hpp
    src/render/compute/compute_pyramid.cpp
    src/render/compute/compute_pyramid.hpp
    src/render/compute/storage_buffer_kernel.cpp
    src/render/compute/storage_buffer_kernel.hpp
    src/render/preview/bezier_patch_preview.cpp
    src/render/preview/bezier_patch_preview.hpp
    src/render/preview/brep_preview.cpp
    src/render/preview/brep_preview.hpp
    src/render/preview/curve_colors.hpp
    src/render/preview/curve_preview.cpp
    src/render/preview/curve_preview.hpp
    src/render/viewport/orbit_camera.cpp
    src/render/viewport/orbit_camera.hpp
    src/render/viewport/viewport_projection.cpp
    src/render/viewport/viewport_projection.hpp
    src/render/vulkan_resources.cpp
    src/render/vulkan_resources.hpp
    src/ui/bezier_patch_inspector.cpp
    src/ui/bezier_patch_inspector.hpp
    src/ui/box_editor.cpp
    src/ui/box_editor.hpp
    src/ui/curve/curve_editor.cpp
    src/ui/curve/curve_editor.hpp
    src/ui/curve/curve_project_panel.cpp
    src/ui/curve/fit_residuals.cpp
    src/ui/curve/fit_residuals.hpp
    src/ui/developer_diagnostics.cpp
    src/ui/developer_diagnostics.hpp
    src/ui/imgui_session.cpp
    src/ui/imgui_session.hpp
    src/ui/project_file_controls.cpp
    src/ui/project_file_controls.hpp
    src/ui/renderer_status_panel.cpp
    src/ui/renderer_status_panel.hpp
)
target_link_libraries(occt_lab PRIVATE occt_lab_geometry TKPrim TKMath SDL3::SDL3 imgui::imgui Vulkan::Vulkan)
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

foreach(name IN ITEMS buffer_check compute_pyramid bezier_patch)
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
    foreach(check IN ITEMS occt_box_check occt_cylinder_check bezier_patch_check bspline_curve_check bspline_surface_check)
        add_executable(${check} tests/${check}/main.cpp)
        target_link_libraries(${check} PRIVATE occt_lab_geometry)
        add_test(NAME ${check} COMMAND ${check})
    endforeach()
    target_link_libraries(occt_box_check PRIVATE TKTopAlgo TKGeomBase)
    target_sources(bezier_patch_check PRIVATE
        tests/bezier_patch_check/patch_project_checks.cpp
        tests/support/project_test_directory.hpp
        src/application/editable_patch.cpp
        src/document/patch_project.cpp
        src/document/project_io.cpp
    )
    target_link_libraries(occt_cylinder_check PRIVATE TKPrim)
    target_sources(bspline_curve_check PRIVATE
        tests/bspline_curve_check/curve_project_checks.cpp
        tests/support/project_test_directory.hpp
        src/application/editable_curve.cpp
        src/document/curve_project.cpp
        src/document/project_io.cpp
    )
endif()
