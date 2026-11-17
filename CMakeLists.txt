cmake_minimum_required(VERSION 3.21)
project(kesLocalizationEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

# Saved files are checked with the engine's own .po parser
set(KES_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../engine")
add_library(kle_engine STATIC
    ${KES_ENGINE_DIR}/po_catalog.cpp
)
target_include_directories(kle_engine PUBLIC ${KES_ENGINE_DIR})
target_compile_definitions(kle_engine PUBLIC KES_PROFILING=0 _CRT_SECURE_NO_WARNINGS NOMINMAX)

add_library(kle_core STATIC
    src/PoFile.h
    src/PoFile.cpp
    src/Languages.h
    src/Languages.cpp
    src/LocProject.h
    src/LocProject.cpp
    src/KeyScanner.h
    src/KeyScanner.cpp
    src/KesProject.h
    src/KesProject.cpp
)
target_include_directories(kle_core PUBLIC src)
target_link_libraries(kle_core PUBLIC kle_engine Qt6::Core)

qt_add_executable(kesLocalizationEditor
    src/main.cpp
    src/KesStyle.h
    src/KesStyle.cpp
    src/StringTableModel.h
    src/StringTableModel.cpp
    src/DetailsPanel.h
    src/DetailsPanel.cpp
    src/MissingKeysPanel.h
    src/MissingKeysPanel.cpp
    src/Dialogs.h
    src/Dialogs.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)
target_link_libraries(kesLocalizationEditor PRIVATE kle_core Qt6::Widgets)

qt_add_resources(kesLocalizationEditor "app_icon"
    PREFIX "/icons"
    BASE resource
    FILES resource/icon.png
)

set_target_properties(kesLocalizationEditor PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

if(MSVC)
    target_compile_options(kesLocalizationEditor PRIVATE /W4 /utf-8)
    target_compile_options(kle_core PRIVATE /W4 /utf-8)
    target_compile_options(kle_engine PRIVATE /utf-8)
else()
    target_compile_options(kesLocalizationEditor PRIVATE -Wall -Wextra)
    target_compile_options(kle_core PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    target_sources(kesLocalizationEditor PRIVATE resource/kesLocalizationEditor.rc)
endif()

install(TARGETS kesLocalizationEditor
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

include(CTest)
if(BUILD_TESTING)
    add_executable(localization_editor_test tests/editor_test.cpp)
    target_link_libraries(localization_editor_test PRIVATE kle_core)
    if(MSVC)
        target_compile_options(localization_editor_test PRIVATE /utf-8)
    endif()
    add_test(NAME localization_editor_test COMMAND localization_editor_test)
    if(WIN32)
        set_tests_properties(localization_editor_test PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${Qt6_DIR}/../../../bin")
    endif()
endif()
