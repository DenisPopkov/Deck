#include "CassetteProfile.h"

namespace cassette
{

namespace
{

void setSideHighCutFromHeadroom(CassetteProfile& p)
{
    p.sideHighCutHz = juce::jmin(20000.0f, p.hfHeadroomKhz * 1000.0f * 0.92f);
}

}

CassetteProfile CassetteProfile::fromFormulation(TapeFormulation f)
{
    CassetteProfile p;
    p.formulation = f;

    switch (f)
    {
        case TapeFormulation::TypeI:
            p.displayName = "Type I (Normal)";
            p.playEqMicroseconds = 120.0f;
            p.hfHeadroomKhz = 11.0f;
            p.maxIntegratedLUFS = -11.5f;
            p.truePeakCeilingDbfs = -1.0f;
            p.sideHighCutHz = 15000.0f;
            p.hfTamerRatio = 1.08f;
            p.hfTamerMaxRatio = 1.45f;
            p.saturationDrive = 0.92f;
            p.applyTapeSaturation = false;
            break;
        case TapeFormulation::TypeII:
            p.displayName = "Type II (Chrome)";
            p.playEqMicroseconds = 70.0f;
            p.hfHeadroomKhz = 15.5f;
            p.maxIntegratedLUFS = -12.0f;
            p.truePeakCeilingDbfs = -1.0f;
            p.emulateHxPro = true;
            p.biasReductionOnHf = 0.08f;
            p.hfTamerRatio = 1.15f;
            p.hfTamerMaxRatio = 2.0f;
            setSideHighCutFromHeadroom(p);
            break;
        case TapeFormulation::TypeIV:
            p.displayName = "Type IV (Metal)";
            p.playEqMicroseconds = 70.0f;
            p.hfHeadroomKhz = 18.5f;
            p.maxIntegratedLUFS = -11.5f;
            p.truePeakCeilingDbfs = -0.1f;
            p.emulateHxPro = true;
            p.biasReductionOnHf = 0.05f;
            p.hfTamerRatio = 1.1f;
            p.hfTamerMaxRatio = 1.8f;
            p.saturationDrive = 1.0f;
            setSideHighCutFromHeadroom(p);
            break;
        case TapeFormulation::VintageWalkman:
            p.displayName = "Vintage Walkman";
            p.playEqMicroseconds = 120.0f;
            p.hfHeadroomKhz = 8.0f;
            p.maxIntegratedLUFS = -12.0f;
            p.truePeakCeilingDbfs = -0.5f;
            p.sideHighCutHz = 8000.0f;
            p.hfTamerRatio = 1.4f;
            p.hfTamerMaxRatio = 2.5f;
            p.applyTapeSaturation = true;
            p.saturationDrive = 0.9f;
            p.gentleTransientShaping = true;
            p.addTapeHissDither = true;
            p.addMicroWowMasking = true;
            p.tapeHissLevelDbfs = -65.0f;
            break;
        case TapeFormulation::HighEndDeck:
            p.displayName = "High-end Deck";
            p.playEqMicroseconds = 70.0f;
            p.hfHeadroomKhz = 16.0f;
            p.maxIntegratedLUFS = -12.5f;
            p.truePeakCeilingDbfs = -1.0f;
            p.emulateHxPro = true;
            p.hfTamerRatio = 1.1f;
            p.hfTamerMaxRatio = 1.7f;
            setSideHighCutFromHeadroom(p);
            break;
        case TapeFormulation::CheapPortable:
            p.displayName = "Cheap Portable Player";
            p.playEqMicroseconds = 120.0f;
            p.hfHeadroomKhz = 10.0f;
            p.maxIntegratedLUFS = -10.5f;
            p.truePeakCeilingDbfs = -0.2f;
            p.hfTamerRatio = 1.3f;
            p.applyTapeSaturation = true;
            p.saturationDrive = 1.02f;
            p.gentleTransientShaping = true;
            p.addTapeHissDither = true;
            p.tapeHissLevelDbfs = -60.0f;
            setSideHighCutFromHeadroom(p);
            break;
    }

    return p;
}

void applyKenwoodKX1100GTuning(CassetteProfile& p)
{
    p.recordingDeck = RecordingDeck::KenwoodKX1100G;
    p.autoPrepRelief = 0.18f;
    p.sideLowCutHz = 110.0f;
    p.hpfCutoffHz = 22.0f;
    p.hfTamerThresholdOffsetDb = -10.0f;
    p.emulateHxPro = false;

    switch (p.formulation)
    {
        case TapeFormulation::TypeI:
            p.displayName = "Type I - Kenwood KX-1100G";
            p.hfHeadroomKhz = 17.0f;
            p.sideHighCutHz = 16000.0f;
            p.maxIntegratedLUFS = -11.0f;
            p.truePeakCeilingDbfs = -0.8f;
            p.hfTamerRatio = 1.06f;
            p.hfTamerMaxRatio = 1.38f;
            break;
        case TapeFormulation::TypeII:
            p.displayName = "Type II - Kenwood KX-1100G";
            p.hfHeadroomKhz = 18.0f;
            p.sideHighCutHz = 17000.0f;
            p.maxIntegratedLUFS = -11.5f;
            p.truePeakCeilingDbfs = -0.5f;
            p.biasReductionOnHf = 0.10f;
            p.hfTamerRatio = 1.10f;
            p.hfTamerMaxRatio = 1.75f;
            break;
        case TapeFormulation::TypeIV:
            p.displayName = "Type IV - Kenwood KX-1100G";
            p.hfHeadroomKhz = 20.5f;
            p.sideHighCutHz = 19600.0f;
            p.maxIntegratedLUFS = -11.0f;
            p.truePeakCeilingDbfs = -0.2f;
            p.biasReductionOnHf = 0.06f;
            p.hfTamerRatio = 1.06f;
            p.hfTamerMaxRatio = 1.60f;
            break;
        default:
            p.displayName = p.displayName + " - Kenwood KX-1100G";
            break;
    }
}

CassetteProfile CassetteProfile::forRecording(RecordingDeck deck, TapeFormulation tape)
{
    auto profile = fromFormulation(tape);

    if (deck == RecordingDeck::KenwoodKX1100G)
        applyKenwoodKX1100GTuning(profile);

    return profile;
}

}
