cmake_minimum_required(VERSION 3.22)

project(Deck VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CASSETTE_BUILD_ESSENTIA "Link Essentia for offline analysis" OFF)
option(CASSETTE_BUILD_ONNX "Link ONNX Runtime for ML tape saturation" OFF)
option(CASSETTE_BUILD_RUBBERBAND "Link Rubber Band for wow/flutter emulation" OFF)
option(CASSETTE_BUILD_GSTPEAQ "Enable GstPEAQ runtime ODG (needs gstreamer at runtime)" ON)
option(CASSETTE_BUILD_TESTS "Build mastering adequacy tests" ON)
option(CASSETTE_BUILD_PLUGIN "Build VST3/AU Cassette Tape Prep plugin" OFF)
option(CASSETTE_BUILD_BURNER "Build CassetteBurner timeline app" OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

include(cmake/CassetteCore.cmake)
include(cmake/OptionalDeps.cmake)

add_library(cassette_core STATIC ${CASSETTE_CORE_SOURCES})
cassette_configure_core_target(cassette_core)

if(CASSETTE_BUILD_ONNX)
    cassette_find_onnxruntime(CASSETTE_ONNX_FOUND)
    if(CASSETTE_ONNX_FOUND)
        message(STATUS "ONNX Runtime: ${CASSETTE_ONNX_LIBRARY}")
        target_include_directories(cassette_core PRIVATE ${CASSETTE_ONNX_INCLUDE})
        target_link_libraries(cassette_core PRIVATE ${CASSETTE_ONNX_LIBRARY})
        target_compile_definitions(cassette_core PRIVATE CASSETTE_HAS_ONNX=1)
    else()
        message(STATUS "CASSETTE_BUILD_ONNX=ON but ONNX Runtime not found — using STN grid only")
    endif()
endif()

if(CASSETTE_BUILD_RUBBERBAND)
    cassette_find_rubberband(CASSETTE_RUBBERBAND_FOUND)
    if(CASSETTE_RUBBERBAND_FOUND)
        message(STATUS "Rubber Band: ${CASSETTE_RUBBERBAND_LIBRARY}")
        target_include_directories(cassette_core PRIVATE ${CASSETTE_RUBBERBAND_INCLUDE})
        target_link_libraries(cassette_core PRIVATE ${CASSETTE_RUBBERBAND_LIBRARY})
        target_compile_definitions(cassette_core PRIVATE CASSETTE_HAS_RUBBERBAND=1)
    else()
        message(STATUS "CASSETTE_BUILD_RUBBERBAND=ON but librubberband not found — using wow emulator")
    endif()
endif()

set(CASSETTE_APP_ICON "${CMAKE_SOURCE_DIR}/Assets/AppIcon.png")

juce_add_gui_app(Deck
    PRODUCT_NAME "Deck"
    COMPANY_NAME "Deck"
    BUNDLE_ID com.deck.app
    ICON_BIG "${CASSETTE_APP_ICON}"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
)

target_sources(Deck PRIVATE
    Source/Main.cpp
    Source/ui/MainComponent.cpp
    Source/ui/TapeSetupPanel.cpp
    Source/ui/MixtapeBuilderPanel.cpp
    Source/ui/ReadinessPanel.cpp
    Source/ui/WorkflowStrip.cpp
    Source/ui/PreviewMonitorPanel.cpp
    Source/ui/AudioDropTarget.cpp
    Source/ui/MeterPanel.cpp
    Source/ui/CompareWaveformDisplay.cpp
    Source/ui/look/CassetteLook.cpp
    Source/ui/WizardStepStrip.cpp
    Source/ui/TrackListEditor.cpp
    Source/ui/ConfirmDialog.cpp
    Source/ui/DropHeroPanel.cpp
    Source/ui/MoreOptionsPanel.cpp
    Source/threading/AnalysisWorker.cpp
    Source/util/AppLog.cpp
)

target_include_directories(Deck PRIVATE
    Source
    Source/io
    Source/threading
    Source/ui
)

target_compile_definitions(Deck PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(Deck PRIVATE
    cassette_core
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_gui_extra
)

if(CASSETTE_BUILD_ESSENTIA)
    cassette_find_essentia(CASSETTE_ESSENTIA_FOUND)
    if(CASSETTE_ESSENTIA_FOUND)
        message(STATUS "Essentia: ${CASSETTE_ESSENTIA_LIBRARY}")
        target_include_directories(cassette_core PRIVATE ${CASSETTE_ESSENTIA_INCLUDE})
        target_link_libraries(cassette_core PRIVATE ${CASSETTE_ESSENTIA_LIBRARY})
        target_compile_definitions(cassette_core PRIVATE CASSETTE_HAS_ESSENTIA=1)
        target_compile_definitions(Deck PRIVATE CASSETTE_HAS_ESSENTIA=1)
    else()
        message(STATUS "CASSETTE_BUILD_ESSENTIA=ON but Essentia not found — using built-in LoudnessMeter")
    endif()
endif()

if(CASSETTE_BUILD_GSTPEAQ)
    target_compile_definitions(cassette_core PRIVATE CASSETTE_HAS_GSTPEAQ=1)
    target_compile_definitions(Deck PRIVATE CASSETTE_HAS_GSTPEAQ=1)
endif()

juce_generate_juce_header(Deck)

# JUCE rebuilds a square Icon.icns from AppIcon.png; restore squircle after each link.
if(APPLE)
    add_custom_command(
        TARGET Deck
        POST_BUILD
        COMMAND /bin/bash "${CMAKE_SOURCE_DIR}/scripts/apply_app_icon.sh"
                "$<TARGET_FILE_DIR:Deck>/../.."
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:Deck>/../Resources/scripts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/scripts/run_peaq.sh"
                "$<TARGET_FILE_DIR:Deck>/../Resources/scripts/run_peaq.sh"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:Deck>/../Resources/models"
        COMMAND /bin/bash "${CMAKE_SOURCE_DIR}/scripts/copy_stn_onnx_if_present.sh"
                "${CMAKE_SOURCE_DIR}/models/tape_stn.onnx"
                "$<TARGET_FILE_DIR:Deck>/../Resources/models/tape_stn.onnx"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Applying squircle Dock icon + GstPEAQ script"
    )
endif()

if(UNIX AND NOT APPLE)
    add_custom_command(
        TARGET Deck
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:Deck>/scripts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/scripts/run_peaq.sh"
                "$<TARGET_FILE_DIR:Deck>/scripts/run_peaq.sh"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:Deck>/models"
        COMMAND /bin/bash "${CMAKE_SOURCE_DIR}/scripts/copy_stn_onnx_if_present.sh"
                "${CMAKE_SOURCE_DIR}/models/tape_stn.onnx"
                "$<TARGET_FILE_DIR:Deck>/models/tape_stn.onnx"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Bundling GstPEAQ script and optional STN model (Linux)"
    )
endif()

if(CASSETTE_BUILD_TESTS)
    enable_testing()

    add_executable(DeckTests Tests/MasteringTests.cpp)
    cassette_configure_core_target(DeckTests)
    target_sources(DeckTests PRIVATE Tests/TestAppLogStub.cpp)
    target_link_libraries(DeckTests PRIVATE cassette_core)
    if(CASSETTE_BUILD_GSTPEAQ)
        target_compile_definitions(DeckTests PRIVATE CASSETTE_HAS_GSTPEAQ=1)
    endif()
    if(CASSETTE_BUILD_ONNX AND CASSETTE_ONNX_FOUND)
        target_compile_definitions(DeckTests PRIVATE CASSETTE_HAS_ONNX=1)
        target_compile_definitions(DeckTests PRIVATE
            CASSETTE_STN_MODEL="${CMAKE_SOURCE_DIR}/models/tape_stn.onnx")
    endif()
    if(CASSETTE_BUILD_RUBBERBAND AND CASSETTE_RUBBERBAND_FOUND)
        target_compile_definitions(DeckTests PRIVATE CASSETTE_HAS_RUBBERBAND=1)
    endif()

    set(CASSETTE_CHARLI_FIXTURE "${CMAKE_SOURCE_DIR}/Tests/Fixtures/Charli-XCX-SS26-Rock-Music.fixture.flac")
    if(EXISTS "${CASSETTE_CHARLI_FIXTURE}")
        target_compile_definitions(DeckTests PRIVATE
            CASSETTE_CHARLI_FIXTURE="${CASSETTE_CHARLI_FIXTURE}")
    endif()

    add_test(NAME MasteringAdequacy COMMAND DeckTests)

    add_executable(CassetteAnalyzeExport Tools/AnalyzeExport.cpp)
    cassette_configure_core_target(CassetteAnalyzeExport)
    target_sources(CassetteAnalyzeExport PRIVATE Tests/TestAppLogStub.cpp)
    target_link_libraries(CassetteAnalyzeExport PRIVATE cassette_core)

    add_executable(CassetteRenderCompare Tools/RenderCompare.cpp)
    cassette_configure_core_target(CassetteRenderCompare)
    target_link_libraries(CassetteRenderCompare PRIVATE cassette_core)

    set(EXPORT_ORIGINAL "${CMAKE_SOURCE_DIR}/../Experiment/Digital Master.flac")
    set(EXPORT_MASTERED "${CMAKE_SOURCE_DIR}/../Experiment/Digital Master_cassette.wav")
    if(EXISTS "${EXPORT_ORIGINAL}" AND EXISTS "${EXPORT_MASTERED}")
        add_test(NAME ExportDigitalMasterCheck
                 COMMAND CassetteAnalyzeExport "${EXPORT_ORIGINAL}" "${EXPORT_MASTERED}")
    endif()
endif()

if(CASSETTE_BUILD_BURNER)
    juce_add_gui_app(CassetteBurner
        PRODUCT_NAME "CassetteBurner"
        COMPANY_NAME "Deck"
        BUNDLE_ID com.cassetteautomaster.burner
        ICON_BIG "${CASSETTE_APP_ICON}"
        NEEDS_CURL FALSE
        NEEDS_WEB_BROWSER FALSE
    )

    target_sources(CassetteBurner PRIVATE
        Source/MainBurner.cpp
        Source/ui/burner/CassetteBurnerComponent.cpp
        Source/ui/burner/TapeParameterPanel.cpp
        Source/ui/burner/TapeTimelineView.cpp
        Source/ui/burner/TransportBar.cpp
        Source/ui/burner/ProjectSidebar.cpp
        Source/ui/burner/StatusBar.cpp
        Source/ui/burner/AnalogVuMeter.cpp
        Source/ui/look/CassetteBurnerLook.cpp
        Source/ui/TrackListEditor.cpp
        Source/threading/AnalysisWorker.cpp
        Source/util/AppLog.cpp
    )

    target_include_directories(CassetteBurner PRIVATE
        Source
        Source/ui
        Source/ui/burner
        Source/threading
    )

    target_compile_definitions(CassetteBurner PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    )

    target_link_libraries(CassetteBurner PRIVATE
        cassette_core
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_gui_extra
    )

    if(CASSETTE_BUILD_GSTPEAQ)
        target_compile_definitions(CassetteBurner PRIVATE CASSETTE_HAS_GSTPEAQ=1)
    endif()

    juce_generate_juce_header(CassetteBurner)

    if(APPLE)
        add_custom_command(
            TARGET CassetteBurner
            POST_BUILD
            COMMAND /bin/bash "${CMAKE_SOURCE_DIR}/scripts/apply_app_icon.sh"
                    "$<TARGET_FILE_DIR:CassetteBurner>/../.."
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Applying squircle Dock icon (CassetteBurner)"
        )
    endif()
endif()

if(CASSETTE_BUILD_PLUGIN)
    juce_add_plugin(CassetteTapePrep
        VERSION 0.9.0
        COMPANY_NAME "Deck"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Casm
        PLUGIN_CODE Ctpr
        FORMATS AU VST3
        PRODUCT_NAME "Cassette Tape Prep"
    )

    target_sources(CassetteTapePrep PRIVATE
        Source/plugin/CassetteTapePrepProcessor.cpp
        Source/plugin/CassetteTapePrepEditor.cpp
    )

    target_compile_definitions(CassetteTapePrep PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(CassetteTapePrep PRIVATE
        cassette_core
        juce::juce_audio_utils
        juce::juce_audio_processors
    )

    juce_generate_juce_header(CassetteTapePrep)

    if(CASSETTE_BUILD_ESSENTIA AND CASSETTE_ESSENTIA_FOUND)
        target_compile_definitions(CassetteTapePrep PRIVATE CASSETTE_HAS_ESSENTIA=1)
    endif()
    if(CASSETTE_BUILD_ONNX AND CASSETTE_ONNX_FOUND)
        target_compile_definitions(CassetteTapePrep PRIVATE CASSETTE_HAS_ONNX=1)
    endif()
    if(CASSETTE_BUILD_RUBBERBAND AND CASSETTE_RUBBERBAND_FOUND)
        target_compile_definitions(CassetteTapePrep PRIVATE CASSETTE_HAS_RUBBERBAND=1)
    endif()
endif()
