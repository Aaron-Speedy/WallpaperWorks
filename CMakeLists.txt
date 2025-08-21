cmake_minimum_required(VERSION 3.10)
project("wallpaper" C)
file(GLOB MAIN_SOURCES "src/*.c" "src/*.h")
file(GLOB SOKOL_HEADERS "third_party/sokol/*.h")
file(GLOB SOKOL_UTIL "third_party/sokol/util/*.h")
add_executable("${PROJECT_NAME}" "${MAIN_SOURCES}")

add_subdirectory("third_party/libwebp")
add_library("sokol" "${SOKOL_HEADERS}" "${SOKOL_UTIL}")
target_include_directories("sokol"
        PUBLIC
        "third_party/sokol"
        PRIVATE
        "third_party/sokol/util"
)

set_target_properties("sokol" PROPERTIES LINKER_LANGUAGE C)

target_compile_features("${PROJECT_NAME}" PRIVATE c_std_17)

target_link_libraries("${PROJECT_NAME}"
        PRIVATE
        "sokol"
        libwebp
)
