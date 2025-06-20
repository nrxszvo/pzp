cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
project(pgnzstparser)

add_library(pgnzstparser SHARED
    eloWriter.cpp
    parallelParser.cpp
    parseMoves.cpp
    decompress.cpp
    parquetWriter.cpp
    utils.cpp
)

# Include directories
target_include_directories(pgnzstparser PUBLIC
    "${PROJECT_SOURCE_DIR}"
)

# System-specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_PREFIXPATH /opt/homebrew)
    # Make sure RPATH is set correctly for macOS
    set_target_properties(pgnzstparser PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@loader_path"
    )
endif()

# Find and link dependencies
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)
find_package(zstd REQUIRED)
find_package(re2 REQUIRED)

# Link dependencies
target_link_libraries(pgnzstparser
    PRIVATE
        Arrow::arrow_shared
        parquet_shared
        curl
        zstd::libzstd
        re2::re2
)

