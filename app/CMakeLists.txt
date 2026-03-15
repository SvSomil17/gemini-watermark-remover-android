cmake_minimum_required(VERSION 3.18.1)
project("watermark_remover")

# Include stb_image
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/third_party)

add_library(watermark_remover SHARED
        src/main/cpp/src/watermark_engine.cpp
        src/main/cpp/src/alpha_map.cpp
        src/main/cpp/src/blend_modes.cpp
        src/main/cpp/src/adaptive_detector.cpp
        src/main/cpp/src/watermark_config.cpp
        src/main/cpp/src/utils.cpp
)

find_library(log-lib log)

target_include_directories(watermark_remover PRIVATE
        src/main/cpp/include
        src/main/cpp/third_party
)

target_link_libraries(watermark_remover
        ${log-lib}
        android
        jnigraphics
        stb_image
)