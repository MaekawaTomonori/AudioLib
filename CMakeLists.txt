cmake_minimum_required(VERSION 3.24)
project(AudioLib LANGUAGES CXX)

add_library(AudioLib STATIC
    src/AudioLib.cpp
    src/Backend.cpp
    src/Handle.cpp
    src/Internal.cpp
    src/LoadDispatcher.cpp
    src/Loader.cpp
    src/Mixer.cpp
    src/PlaybackHandle.cpp
    src/Repository.cpp
    src/Sound.cpp
    src/StreamingVoice.cpp
    src/StreamRefiller.cpp
    src/TrackHandle.cpp
)
target_compile_features(AudioLib PUBLIC cxx_std_20)
target_include_directories(AudioLib
    PUBLIC include
    # ソースは "include/..."、"src/..."、"vendor/..." のようにリポジトリのルートから include する
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
if(MSVC)
    target_compile_options(AudioLib PRIVATE /utf-8)
endif()
