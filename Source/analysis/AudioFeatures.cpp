#include "AudioFeatures.h"

namespace cassette
{

juce::String AudioFeatures::toDisplayString() const
{
    auto line = [](const juce::String& name, const juce::String& value) {
        return name.paddedRight(' ', 22) + value + "\n";
    };

    juce::String s;
    s << line("Integrated LUFS", juce::String(integratedLUFS, 2) + " LUFS");
    s << line("Short-term max", juce::String(shortTermMaxLUFS, 2) + " LUFS");
    s << line("Short-term min", juce::String(shortTermMinLUFS, 2) + " LUFS");
    s << line("Loudness range", juce::String(loudnessRangeDb, 2) + " LU");
    s << line("True Peak", juce::String(truePeakDbfs, 2) + " dBFS");
    s << line("Sample Peak", juce::String(samplePeakDbfs, 2) + " dBFS");
    s << line("RMS", juce::String(rmsLevelDb, 2) + " dBFS");
    s << line("Crest factor", juce::String(crestFactorDb, 2) + " dB");
    s << line("Dynamic range", juce::String(dynamicRangeDb, 2) + " dB");
    s << line("LF energy <150 Hz", juce::String(lfEnergyRatio * 100.0f, 2) + " %");
    s << line("MF 150 Hz-4 kHz", juce::String(mfEnergyRatio * 100.0f, 2) + " %");
    s << line("HF energy >10 kHz", juce::String(hfEnergyRatio * 100.0f, 2) + " %");
    s << line("Stereo correlation", juce::String(stereoCorrelation, 2));
    s << line("LF corr <150 Hz", juce::String(lfStereoCorrelation, 2));
    s << line("Stereo width", juce::String(stereoWidthPercent, 1) + " %");
    s << line("Noise floor est.", juce::String(noiseFloorDbfs, 1) + " dBFS");
    s << line("Clipping risk", juce::String(clippingPercent, 3) + " %");
    s << line("DC offset", juce::String(dcOffsetDbfs, 1) + " dBFS");
    s << line("Sharpness (acum)", juce::String(psycho.sharpnessAcum, 2));
    s << line("Tonality index", juce::String(psycho.tonalityIndex, 2));
    s << line("HF above masking", juce::String(psycho.hfAboveMaskingDb, 1) + " dB");
    s << line("Roughness (asper)", juce::String(psycho.roughnessAsper, 3));
    s << line("Fluctuation (vacil)", juce::String(psycho.fluctuationVacil, 3));
    s << line("Ringing index", juce::String(psycho.streamingRingingIndex * 100.0f, 1) + " %");
    return s;
}

}
