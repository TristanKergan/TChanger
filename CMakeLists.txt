# minimum versiyon ayarlama sanırım
cmake_minimum_required(VERSION 4.4.2)
# Proje adı
project(Hamzex CXX)
# C++26 standardı
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Release build
set(CMAKE_BUILD_TYPE Release)
# Çıktı
add_library(Hamzex SHARED)
# Kaynak dosyaları ve Modüller
target_sources(Hamzex
    PRIVATE
        src/main.cpp
    PRIVATE FILE_SET CXX_MODULES
    FILES
        src/logger.cpp
        src/modules.cpp
        src/offsets.cpp
        src/memory.cpp
        src/resolver.cpp
        src/schemascan.cpp
        src/attributemanager.cpp
        src/json.cpp
        src/config.cpp
        src/skinconfig.cpp
        src/skinchanger.cpp
        src/fsnhook.cpp
        src/agentchanger.cpp
)
