#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/base/ustring.h"

#include "pluginids.h"
#include "version.h"
#include "processor.h"

#define stringPluginName "normalization-limiter-individual"

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace NormalizationLimiterIndividual;

BEGIN_FACTORY_DEF ("Stan Vrabel", "https://github.com/stanvrabel", "mailto:stan@example.com")

    DEF_CLASS2 (
        INLINE_UID_FROM_FUID(ProcessorUID),
        PClassInfo::kManyInstances,
        kVstAudioEffectClass,
        stringPluginName,
        Vst::kDistributable,
        PlugType::kFxDynamics,
        FULL_VERSION_STR,
        kVstVersionString,
        LimiterProcessor::createInstance)

END_FACTORY
