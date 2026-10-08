target_sources(${PROJECT_NAME} PRIVATE
    cApp.cpp
    cMain.cpp
    flatButton.cpp
    config.cpp
    net.cpp
    wu.cpp
    versions.cpp
    appx.cpp
    keypatch.cpp
    installer.cpp
    pch.cpp
    icon.rc)

target_precompile_headers(${PROJECT_NAME} PRIVATE "pch.h")

# Plain Win32 / C++/WinRT sources are kept away from the wxWidgets precompiled header.
set_source_files_properties(net.cpp wu.cpp versions.cpp appx.cpp keypatch.cpp installer.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
