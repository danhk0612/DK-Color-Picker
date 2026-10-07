cmake_minimum_required(VERSION 3.23)

project(DKColorPicker VERSION 0.1.1 LANGUAGES CXX)

add_executable(DKColorPicker WIN32
    src/main.cpp
    src/color_formats.cpp
    src/color_formats.h
    src/color_tools.cpp
    src/color_tools.h
    src/color_library.cpp
    src/color_library.h
    src/localization.cpp
    src/localization.h
    src/theme.cpp
    src/theme.h
    src/utility_window.cpp
    src/utility_window.h
    src/resource.h
    src/app.rc
)

target_compile_features(DKColorPicker PRIVATE cxx_std_20)
target_compile_definitions(DKColorPicker PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)

target_link_libraries(DKColorPicker PRIVATE
    advapi32
    dwmapi
    gdi32
    shell32
    user32
    uxtheme
)

if(MSVC)
    set_property(TARGET DKColorPicker PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(DKColorPicker PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_link_options(DKColorPicker PRIVATE
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
endif()
