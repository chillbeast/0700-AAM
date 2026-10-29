cmake_minimum_required(VERSION 3.15)

## Headless test runner for the audio engine and the reconstructed AudIO_juce
## objects. Writes results to engine_test_results.txt next to the executable
## and exits with the number of failed checks.
set(TargetName KickEngineTest)

project(${TargetName} VERSION 0.1.0)

configure_file(${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

juce_add_gui_app(${TargetName}
        PRODUCT_NAME ${TargetName}
        COMPANY_NAME MusicTribe
)

if(MSVC)
    # The large unity module exceeds the incremental linker's COMDAT handling.
    target_link_options(${TargetName} PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
endif()

if(MSVC OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(${TargetName} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
    )
endif()

target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(${TargetName} PRIVATE Main.cpp)

target_compile_definitions(${TargetName}
    PRIVATE
        KICK_DESKTOP=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_PROJECT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_VERSION>"
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_ASIO=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_PLUGINHOST_VST3=0
        JUCE_PLUGINHOST_AU=0
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TargetName} PRIVATE "-latomic")
endif()

target_link_libraries(${TargetName}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    PRIVATE
        audio_engine
        hardware # CV programs drive the CV outputs through mt::CV
        contracts
        ipc
        juce::juce_audio_devices
        juce::juce_audio_utils
)
