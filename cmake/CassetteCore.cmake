set(CASSETTE_CORE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/LoudnessMeter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/EssentiaAnalyzer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/EssentiaBridge.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/SpectrumAnalyzer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/TruePeakMeter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/WowFlutterAnalyzer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/AudioFeatures.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/PsychoacousticMetrics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/PerceptualQualityGuard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis/PeaqEvaluator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/dynamics/RoughnessDeEsser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/dynamics/MaskingNoiseGate.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/tape/WowFlutterEmulator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/CassetteAutoMaster.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/CassetteMasteringPlanner.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/CassetteProfile.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/project/MixtapeProject.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/project/SideRenderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/project/FolderMixBuilder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/project/MixtapeEditController.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/filters/BiquadFilter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/stereo/MidSideProcessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/dynamics/DynamicEQ.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/dynamics/TransientShaper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/dynamics/TruePeakLimiter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/ml/TapeAwareSoftClipper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/ml/StnGridModel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/ml/OnnxStnRunner.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/tape/RubberBandWowProcessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/graph/MasteringGraph.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp/graph/AdaptiveMasteringProcessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/audio/PerceptualPlaybackProcessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/audio/PreviewEngine.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/export/PreflightTones.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/export/WavExporter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/io/AudioFileLoader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../Source/io/AudioResampler.cpp
)

if(CASSETTE_ENABLE_PI_TAPE)
    list(APPEND CASSETTE_CORE_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/../Source/io/FtpClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../Source/io/PiTapeSettings.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../Source/io/PiTapeUploader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../Source/io/PiTapeControlClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../Source/io/PiTapeRemoteStatus.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../Source/io/PiTapeSessionState.cpp
    )
endif()

function(cassette_configure_core_target target)
    target_include_directories(${target} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../Source
        ${CMAKE_CURRENT_LIST_DIR}/../Source/analysis
        ${CMAKE_CURRENT_LIST_DIR}/../Source/dsp
        ${CMAKE_CURRENT_LIST_DIR}/../Source/export
        ${CMAKE_CURRENT_LIST_DIR}/../Source/audio
    )
    target_link_libraries(${target} PUBLIC
        juce::juce_core
        juce::juce_data_structures
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_audio_formats
    )
    if(CASSETTE_LEGACY_MACOS)
        target_compile_features(${target} PUBLIC cxx_std_17)
    else()
        target_compile_features(${target} PUBLIC cxx_std_20)
    endif()
endfunction()
