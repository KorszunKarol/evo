cmake_minimum_required(VERSION 3.20)
project(evolution LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/sim/include)

# Find or fetch dependencies
include(FetchContent)

# Option to disable FetchContent (use system packages only)
option(USE_SYSTEM_PACKAGES_ONLY "Only use system packages, fail if not found" OFF)

# Prevent FetchContent from checking remote repos if content already exists
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# EnTT (header-only) - Try system package first
find_package(EnTT QUIET)
if(NOT EnTT_FOUND AND NOT USE_SYSTEM_PACKAGES_ONLY)
    FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.12.2
    )
    FetchContent_MakeAvailable(entt)
elseif(NOT EnTT_FOUND AND USE_SYSTEM_PACKAGES_ONLY)
    message(FATAL_ERROR "EnTT not found and USE_SYSTEM_PACKAGES_ONLY is enabled")
endif()

# spdlog - Try system package first
find_package(spdlog QUIET)
if(NOT spdlog_FOUND AND NOT USE_SYSTEM_PACKAGES_ONLY)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.13.0
    )
    FetchContent_MakeAvailable(spdlog)
elseif(NOT spdlog_FOUND AND USE_SYSTEM_PACKAGES_ONLY)
    message(FATAL_ERROR "spdlog not found and USE_SYSTEM_PACKAGES_ONLY is enabled")
endif()

# GoogleTest - Only needed for tests
option(BUILD_TESTS "Build test executables" ON)
if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND AND NOT USE_SYSTEM_PACKAGES_ONLY)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    elseif(NOT GTest_FOUND AND USE_SYSTEM_PACKAGES_ONLY)
        message(FATAL_ERROR "GTest not found and USE_SYSTEM_PACKAGES_ONLY is enabled")
    endif()
endif()

# Graphics libraries - Only needed for render_client
option(BUILD_RENDER_CLIENT "Build render client executable" ON)
if(BUILD_RENDER_CLIENT)
    # GLFW - Try system package first
    find_package(glfw3 QUIET)
    if(NOT glfw3_FOUND AND NOT USE_SYSTEM_PACKAGES_ONLY)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.3.8
        )
        FetchContent_MakeAvailable(glfw)
    elseif(NOT glfw3_FOUND AND USE_SYSTEM_PACKAGES_ONLY)
        message(FATAL_ERROR "glfw3 not found and USE_SYSTEM_PACKAGES_ONLY is enabled")
    endif()

    # GLM (header-only) - Try system package first
    find_package(glm QUIET)
    if(NOT glm_FOUND AND NOT USE_SYSTEM_PACKAGES_ONLY)
        FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 0.9.9.8
        )
        FetchContent_MakeAvailable(glm)
    elseif(NOT glm_FOUND AND USE_SYSTEM_PACKAGES_ONLY)
        message(FATAL_ERROR "glm not found and USE_SYSTEM_PACKAGES_ONLY is enabled")
    endif()

    # glad loader
    FetchContent_GetProperties(glad)
    if(NOT glad_POPULATED AND NOT USE_SYSTEM_PACKAGES_ONLY)
        FetchContent_Declare(
            glad
            GIT_REPOSITORY https://github.com/Dav1dde/glad.git
            GIT_TAG v0.1.36
        )
        FetchContent_Populate(glad)
    endif()
    if(glad_POPULATED)
        add_subdirectory(${glad_SOURCE_DIR} ${glad_BINARY_DIR})
    endif()

    # Dear ImGui
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED AND NOT USE_SYSTEM_PACKAGES_ONLY)
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.90.2
        )
        FetchContent_Populate(imgui)
    endif()
    if(imgui_POPULATED)
        set(IMGUI_SOURCE_DIR ${imgui_SOURCE_DIR})
        add_library(imgui STATIC
            ${IMGUI_SOURCE_DIR}/imgui.cpp
            ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
            ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
            ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
            ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
            ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        )
        target_include_directories(imgui PUBLIC
            ${IMGUI_SOURCE_DIR}
            ${IMGUI_SOURCE_DIR}/backends
        )
        target_link_libraries(imgui PUBLIC glfw glad)
        target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD=1)
    endif()
endif()

# FlatBuffers (for future genome serialization)
FetchContent_Declare(
    flatbuffers
    GIT_REPOSITORY https://github.com/google/flatbuffers.git
    GIT_TAG v24.3.25
)
FetchContent_MakeAvailable(flatbuffers)

if(TARGET flatbuffers::flatc)
    set(FLATBUFFERS_FLATC_EXECUTABLE $<TARGET_FILE:flatbuffers::flatc>)
elseif(EXISTS "${CMAKE_BINARY_DIR}/bin/flatc")
    set(FLATBUFFERS_FLATC_EXECUTABLE "${CMAKE_BINARY_DIR}/bin/flatc")
else()
    set(FLATBUFFERS_FLATC_EXECUTABLE "flatc")
endif()

# FlatBuffers genome schema code generation
set(GENOME_SCHEMA ${CMAKE_SOURCE_DIR}/proto/genome.fbs)
set(GENOME_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated/genome)
set(GENOME_GENERATED_HEADER ${GENOME_GENERATED_DIR}/genome_generated.h)

add_custom_command(
    OUTPUT ${GENOME_GENERATED_HEADER}
    COMMAND ${FLATBUFFERS_FLATC_EXECUTABLE}
            --cpp
            --gen-object-api
            --scoped-enums
            -o ${GENOME_GENERATED_DIR}
            ${GENOME_SCHEMA}
    DEPENDS ${GENOME_SCHEMA}
    COMMENT "Generating FlatBuffers code for genome schema"
    VERBATIM
)

add_custom_target(genome_schema_codegen ALL
    DEPENDS ${GENOME_GENERATED_HEADER}
)

add_library(genome_schema INTERFACE)
add_dependencies(genome_schema genome_schema_codegen)
target_include_directories(genome_schema INTERFACE ${GENOME_GENERATED_DIR})

add_library(sim_genetics
    sim/src/genetics/rng.cpp
    sim/src/genetics/genome_storage.cpp
    sim/src/genetics/derived_traits.cpp
    sim/src/genetics/phenotype_builder.cpp
    sim/src/genetics/genome_ops.cpp
    sim/src/genetics/brain_mlp.cpp
    sim/src/genetics/brain_neat.cpp
    sim/src/genetics/trait_extraction.cpp
    sim/src/genetics/innovation_database.cpp
    sim/src/genetics/mutation_ops.cpp
    sim/src/genetics/morphology_ops.cpp
)

target_include_directories(sim_genetics PUBLIC
    ${CMAKE_SOURCE_DIR}/sim/include
)

target_link_libraries(sim_genetics PUBLIC
    genome_schema
    flatbuffers
    EnTT::EnTT
    spdlog::spdlog
)

target_compile_options(sim_genetics PRIVATE
    -Wall -Wextra -Wpedantic
)
add_dependencies(sim_genetics genome_schema_codegen)

# Simulation library
add_library(sim_core
    # Core framework
    sim/src/core/scheduler.cpp
    sim/src/core/multi_rate_scheduler.cpp
    sim/src/core/simulation_app.cpp
    sim/src/core/scenario.cpp
    # ECS Systems
    sim/src/systems/brain_inference_system.cpp
    sim/src/systems/metabolism_system.cpp
    sim/src/systems/motor_system.cpp
    sim/src/systems/physics_system.cpp
    sim/src/systems/reproduction_system.cpp
    sim/src/systems/fitness_update_system.cpp
    sim/src/systems/species_index_system.cpp
    sim/src/systems/stats_system.cpp
    sim/src/systems/vision_system.cpp
    sim/src/systems/evolution_system.cpp
    # Telemetry
    sim/src/telemetry/telemetry_system.cpp
    # Environment
    sim/src/environment/environment.cpp
    sim/src/environment/environment_bootstrap.cpp
    sim/src/environment/soil_system.cpp
    sim/src/environment/soil_volume.cpp
    sim/src/environment/plant_systems.cpp
    sim/src/environment/feeding_system.cpp
    # Physics
    sim/src/physics/broad_phase.cpp
    sim/src/physics/narrow_phase.cpp
    sim/src/physics/solver.cpp
    sim/src/physics/simple_backend.cpp
)

target_include_directories(sim_core PUBLIC
    ${CMAKE_SOURCE_DIR}/sim/include
)

target_link_libraries(sim_core PUBLIC
    sim_genetics
    EnTT::EnTT
    spdlog::spdlog
)

# Main executable
add_executable(sim_app
    sim/src/main.cpp
)

target_link_libraries(sim_app PRIVATE
    sim_core
)

if(BUILD_RENDER_CLIENT)
    add_executable(render_client
        client/src/main.cpp
        client/src/terrain_textures.cpp
        client/src/soil_debug_renderer.cpp
    )

    target_include_directories(render_client PRIVATE
        ${CMAKE_SOURCE_DIR}/client/include
    )

    target_link_libraries(render_client PRIVATE
        sim_core
        glfw
        glad
        imgui
        glm::glm
    )

    target_compile_options(render_client PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-volatile
    )
endif()

# Compiler warnings
target_compile_options(sim_core PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter  # Common in callback-heavy code
)

target_compile_options(sim_app PRIVATE
    -Wall -Wextra -Wpedantic
)

# Debug symbols even in Release
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")

if(BUILD_TESTS)
    enable_testing()

    add_executable(sim_tests
    tests/physics/test_narrow_phase.cpp
    tests/physics/test_simple_backend.cpp
    tests/sim/test_fixed_point.cpp
    tests/sim/test_soil_volume.cpp
    tests/genetics/test_trait_extraction.cpp
    tests/genetics/test_compatibility_distance.cpp
    tests/genetics/test_mutation_crossover.cpp
    tests/genetics/test_evolution_engine.cpp
    tests/sim/test_xor_evolution.cpp
    tests/sim/test_fitness_update.cpp
    tests/sim/test_fixtures.cpp
    tests/sim/test_rng_seed_derivation.cpp
    tests/sim/test_fitness_component.cpp
    tests/sim/test_species_clustering.cpp
    tests/sim/test_biome_water_determinism.cpp
    tests/sim/test_plant_species_mapping.cpp
    tests/sim/test_reproduction_determinism.cpp
    tests/sim/test_integration_system_ordering.cpp
    tests/sim/test_performance_cadence.cpp
    tests/sim/test_articulated_body.cpp
    tests/sim/test_feeding_system.cpp
    # tests/sim/test_telemetry_bench.cpp
    tests/sim/test_brain_behavior.cpp
)

target_link_libraries(sim_tests PRIVATE
    sim_core
    GTest::gtest_main
)

target_compile_options(sim_tests PRIVATE -Wall -Wextra -Wpedantic)

    include(GoogleTest)
    gtest_discover_tests(sim_tests)

    # Render client tests (requires both tests and render client)
    if(BUILD_RENDER_CLIENT)
        add_executable(render_client_tests
    tests/client/test_render_fixtures.cpp
    tests/client/test_shader.cpp
    tests/client/test_mesh.cpp
    tests/client/test_color_utils.cpp
    tests/client/test_camera.cpp
    tests/client/test_ecs_extraction.cpp
    tests/client/test_instance_buffer.cpp
    tests/client/test_terrain_shading.cpp
    client/src/terrain_textures.cpp
)

target_link_libraries(render_client_tests PRIVATE
    sim_core
    GTest::gtest_main
    glfw
    glad
    glm::glm
)

target_include_directories(render_client_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/client/src
    ${CMAKE_SOURCE_DIR}/client/include
)

target_compile_options(render_client_tests PRIVATE -Wall -Wextra -Wpedantic)

        # Skip render client tests if OpenGL is not available (e.g., in CI without display)
        # Tests will use GTEST_SKIP if OpenGL context creation fails
        gtest_discover_tests(render_client_tests)
    endif()  # BUILD_RENDER_CLIENT
endif()  # BUILD_TESTS
