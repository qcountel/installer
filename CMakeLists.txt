cmake_minimum_required(VERSION 3.20)
project(MinecraftInstaller VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # static CRT: no VC++ redist needed

# CMake 4.x removed compatibility with cmake_minimum_required(VERSION < 3.5), which
# wxWidgets 3.2.x still declares. This lets them configure anyway.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

include(FetchContent)

# ---- wxWidgets (static) ----
set(wxBUILD_SHARED OFF CACHE BOOL "Build wx as static library" FORCE)
set(wxBUILD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
mark_as_advanced(wxBUILD_SHARED)

if(EXISTS "${CMAKE_SOURCE_DIR}/wxWidgets-3.2.0/CMakeLists.txt")
    add_subdirectory(wxWidgets-3.2.0)
else()
    FetchContent_Declare(wxWidgets
        URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.6/wxWidgets-3.2.6.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(wxWidgets)
endif()

# ---- miniz (unzips .appx packages) ----
FetchContent_Declare(miniz
    URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
    URL_HASH SHA256=ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(miniz)   # the release zip has no CMakeLists.txt: this only downloads it
add_library(miniz STATIC "${miniz_SOURCE_DIR}/miniz.c")
target_include_directories(miniz PUBLIC "${miniz_SOURCE_DIR}")
target_compile_definitions(miniz PUBLIC MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_NO_ZLIB_COMPATIBLE_NAMES)

# The icon is stored in git as Base64 text; decode it once at configure time.
set(ICON_ICO "${CMAKE_SOURCE_DIR}/src/icon/icon.ico")
if(NOT EXISTS "${ICON_ICO}")
    execute_process(COMMAND powershell -NoProfile -Command
        "[IO.File]::WriteAllBytes('${ICON_ICO}', [Convert]::FromBase64String((Get-Content -Raw '${CMAKE_SOURCE_DIR}/src/icon/icon.ico.b64')))"
        RESULT_VARIABLE ICON_RESULT)
    if(NOT ICON_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not decode src/icon/icon.ico.b64")
    endif()
endif()

add_executable(${PROJECT_NAME} WIN32)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "Minecraft Installer")
add_subdirectory(src)

# Registering packages for all users and writing next to the exe require administrator rights.
target_link_options(${PROJECT_NAME} PRIVATE
    "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")

# /utf-8: sources contain Russian text; /bigobj and /permissive- for C++/WinRT.
target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /bigobj /permissive- /EHsc)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE NOMINMAX)

target_link_libraries(${PROJECT_NAME} PRIVATE wx::core wx::base miniz
    shell32 ole32 uuid dwmapi wininet bcrypt advapi32 windowsapp)
