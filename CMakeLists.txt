cmake_minimum_required(VERSION 3.28)
project(Hamzex CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# Shared common static library with C++26 modules
add_library(HamzexCommon STATIC)
set_target_properties(HamzexCommon PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(HamzexCommon
    PUBLIC FILE_SET CXX_MODULES
    FILES
        src/logger.cpp
        src/itemcatalog.cpp
        src/json.cpp
        src/config.cpp
        src/skinconfig.cpp
)

# Shared library for CS2 injection: libHamzex.so
add_library(Hamzex SHARED)
target_sources(Hamzex
    PRIVATE
        src/main.cpp
    PRIVATE FILE_SET CXX_MODULES
    FILES
        src/modules.cpp
        src/offsets.cpp
        src/memory.cpp
        src/resolver.cpp
        src/schemascan.cpp
        src/attributemanager.cpp
        src/runtime_config.cpp
        src/ipc_server.cpp
        src/skinchanger.cpp
        src/fsnhook.cpp
        src/agentchanger.cpp
)
target_link_libraries(Hamzex PRIVATE HamzexCommon)

# Native desktop GUI configurator: hamzex-configurator
add_executable(hamzex-configurator
    src/gui/main.cpp
    src/gui/mainwindow.cpp
    src/gui/ipc_client.cpp
)
target_link_libraries(hamzex-configurator
    PRIVATE
        HamzexCommon
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
)

# Convenience target: 'make configurator' or 'ninja configurator'
add_custom_target(configurator DEPENDS hamzex-configurator)
