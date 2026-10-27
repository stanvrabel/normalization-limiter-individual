#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include <vector>

namespace NormalizationLimiterIndividual {

class LimiterProcessor final : public Steinberg::Vst::AudioEffect {
public:
    LimiterProcessor();
    ~LimiterProcessor() SMTG_OVERRIDE = default;

    static Steinberg::FUnknown* createInstance(void*) { return (Steinberg::Vst::IAudioProcessor*)new LimiterProcessor; }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::uint32  PLUGIN_API getLatencySamples() SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;

private:
    enum ParamIds : Steinberg::Vst::ParamID {
        kInputGain = 0,
        kCeiling,
        kRelease,
        kBalance,
        kParamCount
    };

    void resetState();
    void readAutomation(Steinberg::Vst::IParameterChanges* changes);
    static float dbToLin(double db);

private:
    double sampleRate_ {44100.0};

    int lookaheadN_ {220};
    int delayD_ {219};
    int bufSize_ {0};
    int writeIndex_ {0};

    std::vector<float> delayL_;
    std::vector<float> delayR_;
    std::vector<float> reqGain_;
    std::vector<float> minReq_;

    float boxSum_ {0.0f};
    float relFast_ {1.0f};
    float relSlow_ {1.0f};

    // normalized [0..1]
    double pInput_ {12.0 / 36.0};      // 0 dB
    double pCeiling_ {23.7 / 24.0};    // -0.3 dB
    double pRelease_ {110.0 / 990.0};  // 120 ms
    double pBalance_ {0.5};
};

} // namespace NormalizationLimiterIndividual
