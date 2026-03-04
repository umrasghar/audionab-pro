cmake_minimum_required(VERSION 3.20)
project(AudioNabPro VERSION 2.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# --- Release optimizations: static CRT, full optimization ---
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    add_compile_options(/W3 /utf-8)
    # Release flags
    add_compile_options($<$<CONFIG:Release>:/O2 /GL /GS- /Oi>)
    add_link_options($<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
endif()

# --- Sources ---
file(GLOB APP_SOURCES src/*.cpp src/*.c)
file(GLOB UI_SOURCES src/ui/*.cpp)

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

# --- Compile definitions ---
target_compile_definitions(AudioNabPro PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
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
