#include "processor.h"

#include "public.sdk/source/vst/vstparameters.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace NormalizationLimiterIndividual {

LimiterProcessor::LimiterProcessor() {
    setControllerClass(ProcessorUID); // single-component style
}

tresult PLUGIN_API LimiterProcessor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;

    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

    parameters.addParameter(STR16("Input Gain"), STR16("dB"), 0, pInput_, ParameterInfo::kCanAutomate, kInputGain);
    parameters.addParameter(STR16("Ceiling"), STR16("dB"), 0, pCeiling_, ParameterInfo::kCanAutomate, kCeiling);
    parameters.addParameter(STR16("Release"), STR16("ms"), 0, pRelease_, ParameterInfo::kCanAutomate, kRelease);
    parameters.addParameter(STR16("Rhythm<>Harmonics"), nullptr, 0, pBalance_, ParameterInfo::kCanAutomate, kBalance);

    return kResultOk;
}

tresult PLUGIN_API LimiterProcessor::terminate() {
    return AudioEffect::terminate();
}

tresult PLUGIN_API LimiterProcessor::setActive(TBool state) {
    if (state)
        resetState();
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API LimiterProcessor::setupProcessing(ProcessSetup& setup) {
    sampleRate_ = setup.sampleRate;

    lookaheadN_ = std::max(2, static_cast<int>(std::lround(0.005 * sampleRate_))); // ~5 ms
    delayD_ = lookaheadN_ - 1;

    bufSize_ = lookaheadN_ * 4 + 8;
    delayL_.assign(bufSize_, 0.f);
    delayR_.assign(bufSize_, 0.f);
    reqGain_.assign(bufSize_, 1.f);
    minReq_.assign(bufSize_, 1.f);

    resetState();

    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API LimiterProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
    return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

uint32 PLUGIN_API LimiterProcessor::getLatencySamples() {
    return static_cast<uint32>(delayD_);
}

float LimiterProcessor::dbToLin(double db) {
    return static_cast<float>(std::pow(10.0, db / 20.0));
}

void LimiterProcessor::readAutomation(IParameterChanges* changes) {
    if (!changes) return;

    const int32 count = changes->getParameterCount();
    for (int32 i = 0; i < count; ++i) {
        IParamValueQueue* queue = changes->getParameterData(i);
        if (!queue) continue;

        const int32 points = queue->getPointCount();
        if (points <= 0) continue;

        int32 sampleOffset = 0;
        ParamValue value = 0.0;
        if (queue->getPoint(points - 1, sampleOffset, value) != kResultOk)
            continue;

        switch (queue->getParameterId()) {
            case kInputGain: pInput_ = value; break;
            case kCeiling: pCeiling_ = value; break;
            case kRelease: pRelease_ = value; break;
            case kBalance: pBalance_ = value; break;
            default: break;
        }
    }
}

tresult PLUGIN_API LimiterProcessor::process(ProcessData& data) {
    readAutomation(data.inputParameterChanges);

    if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0)
        return kResultOk;

    AudioBusBuffers& inBus = data.inputs[0];
    AudioBusBuffers& outBus = data.outputs[0];

    if (inBus.numChannels < 2 || outBus.numChannels < 2)
        return kResultOk;

    float* inL = inBus.channelBuffers32[0];
    float* inR = inBus.channelBuffers32[1];
    float* outL = outBus.channelBuffers32[0];
    float* outR = outBus.channelBuffers32[1];

    if (!inL || !inR || !outL || !outR)
        return kResultOk;

    const float inputGain = dbToLin(-12.0 + 36.0 * pInput_);
    const float ceiling = dbToLin(-24.0 + 24.0 * pCeiling_);

    const double releaseMs = 10.0 + 990.0 * pRelease_;
    const float balance = static_cast<float>(pBalance_);

    // Fast (rhythm) and slow (harmonics) release constants
    const double tFastSec = std::max(1.0, releaseMs * 0.2) * 0.001;
    const double tSlowSec = (releaseMs * 5.0) * 0.001;

    const float cFast = static_cast<float>(1.0 - std::exp(-1.0 / (tFastSec * sampleRate_)));
    const float cSlow = static_cast<float>(1.0 - std::exp(-1.0 / (tSlowSec * sampleRate_)));

    for (int32 s = 0; s < data.numSamples; ++s) {
        const float l = inL[s] * inputGain;
        const float r = inR[s] * inputGain;

        const float peak = std::max(std::fabs(l), std::fabs(r));
        const float need = (peak > ceiling) ? (ceiling / peak) : 1.0f;

        reqGain_[writeIndex_] = need;
        delayL_[writeIndex_] = l;
        delayR_[writeIndex_] = r;

        // Sliding minimum over lookahead window
        float m = 1.0f;
        for (int k = 0; k < lookaheadN_; ++k) {
            int idx = writeIndex_ - k;
            if (idx < 0) idx += bufSize_;
            m = std::min(m, reqGain_[idx]);
        }
        minReq_[writeIndex_] = m;

        // Box smoothing over the min signal (smooth attack without overshoot)
        boxSum_ += m;
        int oldIdx = writeIndex_ - lookaheadN_;
        if (oldIdx < 0) oldIdx += bufSize_;
        boxSum_ -= minReq_[oldIdx];

        const float smooth = std::min(1.0f, boxSum_ / static_cast<float>(lookaheadN_));

        // Dual release followers; both constrained by smooth when attacking
        relFast_ = (smooth < relFast_) ? smooth : (relFast_ + (smooth - relFast_) * cFast);
        relSlow_ = (smooth < relSlow_) ? smooth : (relSlow_ + (smooth - relSlow_) * cSlow);

        // Blend rhythm/harmonics release paths
        const float gain = (1.0f - balance) * relFast_ + balance * relSlow_;

        // Apply to delayed audio
        int readIdx = writeIndex_ - delayD_;
        if (readIdx < 0) readIdx += bufSize_;

        float oL = delayL_[readIdx] * gain;
        float oR = delayR_[readIdx] * gain;

        // Emergency stage: hard safety against ceiling overshoot
        const float outPeak = std::max(std::fabs(oL), std::fabs(oR));
        if (outPeak > ceiling) {
            const float e = ceiling / outPeak;
            oL *= e;
            oR *= e;
        }

        outL[s] = oL;
        outR[s] = oR;

        ++writeIndex_;
        if (writeIndex_ >= bufSize_) writeIndex_ = 0;
    }

    return kResultOk;
}

tresult PLUGIN_API LimiterProcessor::setState(IBStream* state) {
    if (!state) return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);
    double values[kParamCount] {};

    for (int i = 0; i < kParamCount; ++i) {
        if (!streamer.readDouble(values[i]))
            return kResultFalse;
    }

    pInput_ = values[kInputGain];
    pCeiling_ = values[kCeiling];
    pRelease_ = values[kRelease];
    pBalance_ = values[kBalance];

    return kResultOk;
}

tresult PLUGIN_API LimiterProcessor::getState(IBStream* state) {
    if (!state) return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);
    streamer.writeDouble(pInput_);
    streamer.writeDouble(pCeiling_);
    streamer.writeDouble(pRelease_);
    streamer.writeDouble(pBalance_);
    return kResultOk;
}

void LimiterProcessor::resetState() {
    std::fill(delayL_.begin(), delayL_.end(), 0.0f);
    std::fill(delayR_.begin(), delayR_.end(), 0.0f);
    std::fill(reqGain_.begin(), reqGain_.end(), 1.0f);
    std::fill(minReq_.begin(), minReq_.end(), 1.0f);

    writeIndex_ = 0;
    boxSum_ = static_cast<float>(lookaheadN_);
    relFast_ = 1.0f;
    relSlow_ = 1.0f;
}

} // namespace NormalizationLimiterIndividual
