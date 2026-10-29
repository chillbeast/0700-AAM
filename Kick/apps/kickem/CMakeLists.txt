cmake_minimum_required(VERSION 3.15)

## KickEM - software simulator of the KICK front panel with the real Kick UI embedded.
## Enabled with -DKICK_SIMULATOR=ON (see apps/CMakeLists.txt).

## Community simulator releases have their own version; device firmware retains its upstream version.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" ver)

string(REGEX MATCH "VERSION_MAJOR ([0-9]*)" _ ${ver})
set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_MINOR ([0-9]*)" _ ${ver})
set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_PATCH ([0-9]*)" _ ${ver})
set(PROJECT_VERSION_PATCH ${CMAKE_MATCH_1})

set(TargetName     KT_KickEM)
set(TargetCompany  MusicTribe)
set(TargetVersion  ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

project(${TargetName} VERSION ${TargetVersion})

## Runtime data (assets/templates/demos) is looked up next to the executable
## (see modules/common/source/kick_paths.cpp). apps/kick normally copies them already.
foreach(dataDir assets templates demos)
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/${dataDir}")
        file(COPY "${CMAKE_SOURCE_DIR}/${dataDir}" DESTINATION "${CMAKE_BINARY_DIR}")
    endif()
endforeach()
set(TargetSrc_DIR ".")

## Generate configuration file (config.h is included by the common module)...
configure_file(${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

juce_add_gui_app(${TargetName}
        PRODUCT_NAME ${TargetName}
        COMPANY_NAME ${TargetCompany}
)

if(WIN32)
    # JUCE 7's resource-generation rule omits Info.txt, leaving the old Windows
    # file version in incremental builds after a community version change.
    get_target_property(panel_generated_dir ${TargetName} JUCE_GENERATED_SOURCES_DIRECTORY)
    get_target_property(panel_info_file ${TargetName} JUCE_INFO_FILE)
    add_custom_command(OUTPUT "${panel_generated_dir}/${TargetName}_resources.rc"
        APPEND DEPENDS "${panel_info_file}" "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")
endif()

juce_add_binary_data(KickPanelArtwork
    NAMESPACE KickPanelData
    HEADER_NAME KickPanelData.h
    SOURCES artwork/kick-panel.svg
)
target_link_libraries(${TargetName} PRIVATE KickPanelArtwork)

set_target_properties(${TargetName} PROPERTIES
        CXX_STANDARD                    17
        VS_DEBUGGER_WORKING_DIRECTORY   "${CMAKE_BINARY_DIR}"
)

if(MSVC OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(${TargetName} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<0:>
    )
endif()

target_include_directories(${TargetName}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        # Re-use the monolith adapters (device / audio engine / audio interface) of apps/kick.
        ${CMAKE_CURRENT_SOURCE_DIR}/../kick
)

target_sources(${TargetName}
    PRIVATE
        Main.cpp
        simulator/kick_Simulator.h
        simulator/kick_Simulator.cpp
        simulator/kick_SimulatorControls.h
        simulator/kick_SimulatorControls.cpp
        simulator/kick_SimulatorMainControl.h
        simulator/kick_SimulatorMainControl.cpp
        simulator/kick_SimulatorInterfaceImpl.h
        simulator/kick_SimulatorInterfaceImpl.cpp
        simulator/kick_PanelArtwork.h
        simulator/kick_PanelArtwork.cpp
        simulator/kick_MidiLearn.h
        simulator/kick_MidiLearn.cpp
        simulator/kick_SimulatorMidi.cpp
        simulator/kick_MidiAuditTests.cpp
)

target_compile_definitions(${TargetName}
    PRIVATE
        SIM_BUILD=1
        KICK_DESKTOP=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_PROJECT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TargetName},JUCE_VERSION>"
        JUCE_ALSA=1
        JUCE_JACK=1

        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCER_ENABLE_GPL_MODE=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_ENABLE_REPAINT_DEBUGGING=0

        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1

        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_VST3=${USE_VST}
        JUCE_PLUGINHOST_AU=${USE_AU}

        THPAR_SQLITE=1
        USE_OPTICK=${USE_OPTICK_VAL}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TargetName} PRIVATE "-latomic")
endif()

target_link_libraries(${TargetName}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags

    PRIVATE
        audio_engine
        kick_ui
        program_edit_ui
        juce::juce_gui_extra
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        contracts
        ipc
)

if(PERFETTO)
    target_link_libraries(${TargetName} PRIVATE Melatonin::Perfetto)
endif()

if(TIMESTAMP)
    target_link_libraries(${TargetName} PRIVATE timestamp)
endif()

if(USE_ASAN_KICK)
   target_link_libraries(${TargetName} PRIVATE libsancov.lib)
   target_link_options(${TargetName}  PRIVATE /fsanitize=address)
   target_compile_options(${TargetName} PRIVATE /fsanitize=address /Od)
endif()

# ASIO devices when KICK_ASIO_SDK_DIR is set (see Kick/CMakeLists.txt).
kick_enable_asio( ${TargetName} )
