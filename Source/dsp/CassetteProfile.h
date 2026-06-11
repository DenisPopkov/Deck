#pragma once

#include <juce_core/juce_core.h>

namespace cassette
{

enum class TapeFormulation
{
    TypeI,
    TypeII,
    TypeIV,
    VintageWalkman,
    HighEndDeck,
    CheapPortable
};

enum class RecordingDeck
{
    Generic,
    KenwoodKX1100G
};

struct CassetteProfile
{
    TapeFormulation formulation = TapeFormulation::TypeII;
    RecordingDeck recordingDeck = RecordingDeck::Generic;
    juce::String displayName;
    float playEqMicroseconds = 70.0f;
    float hfHeadroomKhz = 15.5f;
    float maxIntegratedLUFS = -12.0f;
    float truePeakCeilingDbfs = -0.3f;
    float hpfCutoffHz = 25.0f;
    float sideLowCutHz = 120.0f;
    float sideHighCutHz = 12000.0f;
    float hfTamerRatio = 1.15f;
    float hfTamerMaxRatio = 2.0f;
    float hfTamerThresholdOffsetDb = -9.0f;
    float saturationDrive = 1.0f;
    float biasReductionOnHf = 0.0f;
    float autoPrepRelief = 0.0f;
    bool emulateHxPro = false;
    bool applyTapeSaturation = false;
    bool gentleTransientShaping = false;
    bool addTapeHissDither = false;
    bool addMicroWowMasking = false;
    float tapeHissLevelDbfs = -70.0f;

    static CassetteProfile fromFormulation(TapeFormulation f);
    static CassetteProfile forRecording(RecordingDeck deck, TapeFormulation tape);
};

}
