cmake_minimum_required(VERSION 3.28)
project(CMakeSFMLProject LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)

# --- DISABLE AUDIO, NETWORK & GRAPHICS ---
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_GRAPHICS OFF CACHE BOOL "" FORCE)
# --------------------------------------------------
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(stb)

find_package(OpenGL REQUIRED)

add_executable(robot_arm
    src/main.cpp
    src/geometry.cpp
    src/mesh.cpp
    src/shader.cpp
    src/opengl_loader.cpp
)

# Tell compiler where to find gl.h
target_include_directories(robot_arm PRIVATE
    include
    ${stb_SOURCE_DIR}
)

target_compile_features(robot_arm PRIVATE cxx_std_17)

target_link_libraries(robot_arm
    PRIVATE
        SFML::Window
        SFML::System
        glm::glm
        OpenGL::GL
)

# Put the shaders and textures next to the executable
add_custom_command(TARGET robot_arm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:robot_arm>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mesh.vert"
        "$<TARGET_FILE_DIR:robot_arm>/shaders/mesh.vert"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mesh.frag"
        "$<TARGET_FILE_DIR:robot_arm>/shaders/mesh.frag"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:robot_arm>/assets"
    VERBATIM
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT robot_arm)
