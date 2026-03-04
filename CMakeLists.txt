cmake_minimum_required(VERSION 3.20)
project(AudioNabPro VERSION 2.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# --- Static CRT ---
if(MSVC)
    # Use static runtime (/MT for Release, /MTd for Debug)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W3 /utf-8)

    # Release optimizations (applied directly for single-config generators)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /GL /GS- /Oi)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    endif()
endif()

# --- Sources (explicit lists to avoid pulling in unwanted files like shell.c) ---
set(APP_SOURCES
    src/main.cpp
    src/app.cpp
    src/converter.cpp
    src/config.cpp
    src/database.cpp
    src/helpers.cpp
)

set(UI_SOURCES
    src/ui/renderer.cpp
    src/ui/controls.cpp
)

# NOTE: Do NOT add third_party/sqlite3/shell.c — it contains its own main()
# which conflicts with our WinMain entry point.
set(THIRD_PARTY_SOURCES
    third_party/sqlite3/sqlite3.c
    third_party/cjson/cJSON.c
)

# --- Executable (WIN32 = no console window) ---
add_executable(AudioNabPro WIN32
    ${APP_SOURCES}
    ${UI_SOURCES}
    ${THIRD_PARTY_SOURCES}
    audionab.rc
)

# Disable CMake's auto-generated manifest — we embed ours via .rc file
set_target_properties(AudioNabPro PROPERTIES LINK_FLAGS "/MANIFEST:NO")

# --- Compile definitions ---
target_compile_definitions(AudioNabPro PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    # SQLite optimizations
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
    SQLITE_DQS=0
)

# --- Include directories ---
target_include_directories(AudioNabPro PRIVATE
    src
    third_party/sqlite3
    third_party/cjson
    third_party/ffmpeg/include
)

# --- Link Windows system libraries ---
target_link_libraries(AudioNabPro PRIVATE
    # Direct2D + DirectWrite (UI rendering)
    d2d1
    dwrite
    # Window management
    user32
    gdi32
    dwmapi
    # Shell (tray, file dialogs, DnD)
    shell32
    shlwapi
    ole32
    comctl32
    comdlg32
    # Registry
    advapi32
    # HTTP (Deepgram API)
    winhttp
)

# --- FFmpeg static libraries (when available) ---
set(FFMPEG_LIB_DIR ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/lib)
if(EXISTS ${FFMPEG_LIB_DIR}/avformat.lib)
    target_link_libraries(AudioNabPro PRIVATE
        ${FFMPEG_LIB_DIR}/avformat.lib
        ${FFMPEG_LIB_DIR}/avcodec.lib
        ${FFMPEG_LIB_DIR}/avutil.lib
        ${FFMPEG_LIB_DIR}/swresample.lib
    )
    # External codec libs
    if(EXISTS ${FFMPEG_LIB_DIR}/libmp3lame.lib)
        target_link_libraries(AudioNabPro PRIVATE ${FFMPEG_LIB_DIR}/libmp3lame.lib)
    endif()
    if(EXISTS ${FFMPEG_LIB_DIR}/libopus.lib)
        target_link_libraries(AudioNabPro PRIVATE ${FFMPEG_LIB_DIR}/libopus.lib)
    endif()
    target_compile_definitions(AudioNabPro PRIVATE HAS_FFMPEG_LIBS=1)
    # FFmpeg needs these Windows libs
    target_link_libraries(AudioNabPro PRIVATE bcrypt mfplat mfuuid strmiids ws2_32 secur32)
else()
    message(STATUS "FFmpeg static libs not found - converter will use external ffmpeg.exe as fallback")
endif()
