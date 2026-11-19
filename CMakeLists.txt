cmake_minimum_required(VERSION 3.21)
project(occt_lab LANGUAGES C CXX)

find_package(OpenCASCADE CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(slang CONFIG REQUIRED)

add_library(occt_lab_dependencies INTERFACE)
target_compile_features(occt_lab_dependencies INTERFACE cxx_std_23)
target_include_directories(occt_lab_dependencies SYSTEM INTERFACE
    "${OpenCASCADE_INCLUDE_DIR}"
)
target_link_libraries(occt_lab_dependencies INTERFACE
    ${OpenCASCADE_LIBRARIES}
    SDL3::SDL3
    imgui::imgui
    Vulkan::Vulkan
)

add_executable(occt_lab
    src/main.cpp
    src/vulkan_window.cpp
    src/geometry/box_model.cpp
    src/geometry/brep_mesh.cpp
    src/geometry/bspline_curve.cpp
    src/render/brep_preview.cpp
    src/render/curve_preview.cpp
    src/render/orbit_camera.cpp
)
target_include_directories(occt_lab PRIVATE src)
target_link_libraries(occt_lab PRIVATE occt_lab_dependencies)
target_compile_definitions(occt_lab PRIVATE VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0)
set_target_properties(occt_lab PROPERTIES CXX_EXTENSIONS OFF)

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

add_custom_target(occt_lab_shaders DEPENDS "${triangle_spv}" "${preview_vertex_spv}" "${preview_fragment_spv}")
add_dependencies(occt_lab occt_lab_shaders)

include(CTest)
if(BUILD_TESTING)
    foreach(check IN ITEMS occt_box_check occt_cylinder_check)
        add_executable(${check} tests/${check}.cpp src/geometry/brep_mesh.cpp)
        target_include_directories(${check} SYSTEM PRIVATE "${OpenCASCADE_INCLUDE_DIR}")
        target_link_libraries(${check} PRIVATE TKMesh TKPrim TKTopAlgo TKBRep TKernel)
    endforeach()
    target_sources(occt_box_check PRIVATE src/geometry/box_model.cpp)

    add_executable(bezier_patch_check tests/bezier_patch_check.cpp src/geometry/bezier_patch.cpp)

    add_executable(bspline_curve_check tests/bspline_curve_check.cpp src/geometry/bspline_curve.cpp)
    target_include_directories(bspline_curve_check SYSTEM PRIVATE "${OpenCASCADE_INCLUDE_DIR}")
    target_link_libraries(bspline_curve_check PRIVATE TKG3d TKernel)

    foreach(check IN ITEMS occt_box_check occt_cylinder_check bezier_patch_check bspline_curve_check)
        target_compile_features(${check} PRIVATE cxx_std_23)
        target_include_directories(${check} PRIVATE src)
        set_target_properties(${check} PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME ${check} COMMAND ${check})
    endforeach()
endif()
