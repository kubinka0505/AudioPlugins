#include "Processor.h"
#include "Editor.h"

// -=-=-=- //
// Constructor / destructor
// -=-=-=- //

AudioProcessor::AudioProcessor()
	: juce::AudioProcessor(BusesProperties()
		.withInput ("Input",     juce::AudioChannelSet::stereo(), true)
		.withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
		.withOutput("Output",    juce::AudioChannelSet::stereo(), true)
	),
	apvts(*this, nullptr, "Parameters", createParameterLayout())
{
	delayBpmParam = apvts.getRawParameterValue("delay_bpm");
}

AudioProcessor::~AudioProcessor() {}

// -=-=-=- //
// Preset support
// -=-=-=- //

const juce::String AudioProcessor::getProgramName(int index) {
	const auto& presets = factoryPresets();

	if (index >= 0 && index < (int)presets.size()) {
		return presets[(size_t)index].name;
	}

	return {};
}

void AudioProcessor::setCurrentProgram(int index) {
	const auto& presets = factoryPresets();

	if (index < 0 || index >= (int)presets.size()) {
		return;
	}

	currentPreset = index;

	// First reset every parameter to its default value
	for (auto* p : apvts.processor.getParameters()) {
		if (auto* pf = dynamic_cast<juce::RangedAudioParameter*>(p)) {
			pf -> beginChangeGesture();
			pf -> setValueNotifyingHost(pf -> getDefaultValue());
			pf -> endChangeGesture();
		}
	}

	// Then apply the preset's overrides
	const auto& preset = presets[(size_t)index];
	for (auto [id, val] : preset.values) {
		if (auto* p = apvts.getParameter(id)) {
			p -> beginChangeGesture();
			p -> setValueNotifyingHost(val);
			p -> endChangeGesture();
		}
	}
}

// -=-=-=- //
// Bus layout
// -=-=-=- //

bool AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
	// Main output must be stereo
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
		return false;
	}

	// Main input must be stereo
	if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) {
		return false;
	}

	// Sidechain (bus index 1) may be disabled or stereo
	const auto& sc = layouts.getChannelSet(true, 1);
	if (!sc.isDisabled() && sc != juce::AudioChannelSet::stereo()) {
		return false;
	}

	return true;
}

// -=-=-=- //
// Delay helpers
// -=-=-=- //

juce::String AudioProcessor::delayText(float v) const {
	const bool bpm = delayBpmParam && delayBpmParam -> load() > 0.5f;
	if (!bpm) {
		return GainUtils::sideText(v, DelayMap::kMaxMs, "ms");
	}

	const int n = (int)std::lround(std::abs(v - 0.5f) * 2.0f * DelayMap::kSteps);

	if (n <= 0) {
		return "0 beat";
	}

	return juce::String(v < 0.5f ? "L " : "R ") + DelayMap::kNames[n - 1] + " beat";
}

void AudioProcessor::delayTargets(float v, float out[2]) const {
	const bool  bpm = delayBpmParam && delayBpmParam -> load() > 0.5f;

	const float sec = DelayMap::seconds(v, bpm, lastBpm.load());
	const float smp = juce::jmin(sec * (float)currentSampleRate, (float)(maxDelaySamples - 2));

	out[0] = (v < 0.5f) ? smp : 0.0f;
	out[1] = (v > 0.5f) ? smp : 0.0f;
}

// -=-=-=- //
// Parameter layout
// -=-=-=- //

juce::AudioProcessorValueTreeState::ParameterLayout AudioProcessor::createParameterLayout() {
	juce::AudioProcessorValueTreeState::ParameterLayout layout;
	using P = juce::AudioParameterFloat;
	using A = juce::AudioParameterFloatAttributes;

	const juce::NormalisableRange<float> r(0.0f, 1.0f);

	// Bipolar fader (all matrix sliders share the same curve and display format)
	auto addFader = [&](const char* id, const char* name, float def) {
		layout.add(std::make_unique<P>(
			juce::ParameterID{ id, 1 }, name, r, def,
			A().withStringFromValueFunction([](float v, int) {
				return GainUtils::gainText(v);
			})));
	};

	// Main matrix
	addFader("ril", "RIL", 0.5f);
	addFader("l",   "L",   0.75f);
	addFader("r",   "R",   0.75f);
	addFader("lir", "LIR", 0.5f);

	// Delay knob + BPM toggle
	layout.add(std::make_unique<P>(
		juce::ParameterID{ "delay", 1 }, "Delay", r, 0.5f,
		A().withStringFromValueFunction([this](float v, int) {
			return delayText(v);
		})));
	layout.add(std::make_unique<juce::AudioParameterBool>(
		juce::ParameterID{ "delay_bpm", 1 }, "Delay BPM", false));

	// Phase shifter
	layout.add(std::make_unique<P>(
		juce::ParameterID{ "phase", 1 }, "Phase", r, 0.5f,
		A().withStringFromValueFunction([](float v, int) {
			return GainUtils::sideText(v, 180.0f, juce::String::fromUTF8("\xc2\xb0"));
		})));

	// Pan
	layout.add(std::make_unique<P>(
		juce::ParameterID{ "pan", 1 }, "Pan", r, 0.5f,
		A().withStringFromValueFunction([](float v, int) {
			return GainUtils::sideText(v, 1.0f, "");
		})));

	// Stereo separation
	layout.add(std::make_unique<P>(
		juce::ParameterID{ "sep", 1 }, "Separation", r, 0.5f,
		A().withStringFromValueFunction([](float v, int) {
			return juce::String(v * 200.0f, 3) + " %";
		})));

	// PCE / PCR rows (same bipolar curve, default = silence)
	for (const char* prefix : { "pce_", "pcr_" }) {
		for (const char* p_ch : { "ril", "l", "r", "lir" }) {
			const juce::String id   = juce::String(prefix) + p_ch;
			const juce::String name = juce::String(prefix).toUpperCase() + p_ch;

			addFader(
				id.toRawUTF8(),
				name.toRawUTF8(),
				0.5f
			);
		}
	}

	return layout;
}

// -=-=-=- //
// Prepare / release
// -=-=-=- //

void AudioProcessor::prepareToPlay(double sampleRate, int) {
	currentSampleRate = sampleRate;
	maxDelaySamples   = static_cast<int>(10.0 * sampleRate) + 8;

	// Rebuild the stereo mask engine with the currently selected FFT size
	const int wantedFft = pcFftSize.load();

	pc.prepare(sampleRate, wantedFft);
	activeFftSize.store(pc.getLatency()); // getLatency() == fftN after prepare()

	const int pcLatency = pc.getLatency();

	for (auto& c : ch) {
		c.apState = 0.0f;

		c.latBuf.assign((size_t)pcLatency, 0.0f);
		c.lpos = 0;

		c.dbuf.assign((size_t)maxDelaySamples, 0.0f);
		c.wpos = 0;
	}

	setLatencySamples(pcLatency);

	auto get = [&](const char* id) {
		return apvts.getRawParameterValue(id) -> load();
	};

	auto initSmooth = [&](juce::SmoothedValue<float>& s, float v) {
		s.reset(sampleRate, 0.02);
		s.setCurrentAndTargetValue(v);
	};

	initSmooth(gLir,        GainUtils::volToGain(get("lir")));
	initSmooth(gL,          GainUtils::volToGain(get("l")));
	initSmooth(gR,          GainUtils::volToGain(get("r")));
	initSmooth(gRil,        GainUtils::volToGain(get("ril")));
	initSmooth(widthSmooth, get("sep") * 2.0f);
	initSmooth(panSmooth,   get("pan"));

	float dt[2];
	delayTargets(get("delay"), dt);
	initSmooth(delaySmooth[0], dt[0]);
	initSmooth(delaySmooth[1], dt[1]);

	const float p    = get("phase");
	const float pAmt = std::abs(p - 0.5f) * 2.0f;
	initSmooth(phaseSmooth[0], p < 0.5f ? pAmt * juce::MathConstants<float>::pi : 0.0f);
	initSmooth(phaseSmooth[1], p > 0.5f ? pAmt * juce::MathConstants<float>::pi : 0.0f);

	const char* pceIds[] = { "pce_ril","pce_l","pce_r","pce_lir" };
	const char* pcrIds[] = { "pcr_ril","pcr_l","pcr_r","pcr_lir" };

	for (int i = 0; i < 4; ++i) {
		initSmooth(gPce[i], GainUtils::volToGain(get(pceIds[i])));
		initSmooth(gPcr[i], GainUtils::volToGain(get(pcrIds[i])));
	}
}

void AudioProcessor::releaseResources() {}

// -=-=-=- //
// Process block
// -=-=-=- //

void AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
	juce::ScopedNoDenormals noDenormals;
	if (getTotalNumInputChannels() < 2 || getTotalNumOutputChannels() < 2) {
		return;
	}

	// rebuild
	const int wanted = pcFftSize.load();

	if (wanted != activeFftSize.load()) {
		pc.prepare(currentSampleRate, wanted);

		const int newLat = pc.getLatency();
		activeFftSize.store(newLat);

		for (auto& c : ch) {
			c.latBuf.assign((size_t)newLat, 0.0f);
			c.lpos = 0;
		}

		setLatencySamples(newLat);
	}

	auto get = [&](const char* id) {
		return apvts.getRawParameterValue(id) -> load();
	};

	// Update gain smoother targets
	gLir.setTargetValue(GainUtils::volToGain(get("lir")));
	gL  .setTargetValue(GainUtils::volToGain(get("l")));
	gR  .setTargetValue(GainUtils::volToGain(get("r")));
	gRil.setTargetValue(GainUtils::volToGain(get("ril")));
	widthSmooth.setTargetValue(get("sep") * 2.0f);
	panSmooth  .setTargetValue(get("pan"));

	// Pull BPM from host play-head (non-blocking)
	if (auto* ph = getPlayHead()) {
		if (auto pos = ph -> getPosition()) {
			if (auto bpm = pos -> getBpm()) {
				if (*bpm > 0.0) {
					lastBpm.store(*bpm);
				}
			}
		}
	}

	float dt[2];
	delayTargets(get("delay"), dt);
	delaySmooth[0].setTargetValue(dt[0]);
	delaySmooth[1].setTargetValue(dt[1]);

	const float p    = get("phase");
	const float pAmt = std::abs(p - 0.5f) * 2.0f;
	phaseSmooth[0].setTargetValue(p < 0.5f ? pAmt * juce::MathConstants<float>::pi : 0.0f);
	phaseSmooth[1].setTargetValue(p > 0.5f ? pAmt * juce::MathConstants<float>::pi : 0.0f);

	const char* pceIds[] = { "pce_ril","pce_l","pce_r","pce_lir" };
	const char* pcrIds[] = { "pcr_ril","pcr_l","pcr_r","pcr_lir" };

	for (int i = 0; i < 4; ++i) {
		gPce[i].setTargetValue(GainUtils::volToGain(get(pceIds[i])));
		gPcr[i].setTargetValue(GainUtils::volToGain(get(pcrIds[i])));
	}

	float* data[2] = {
		buffer.getWritePointer(0),
		buffer.getWritePointer(1)
	};

	for (int n = 0; n < buffer.getNumSamples(); ++n) {
		// Stereo mask split
		float pceM, pcrL, pcrR;
		pc.process(
			data[0][n],
			data[1][n],

			pceM,

			pcrL,
			pcrR
		);

		const float pce[2] = { pceM, pceM };
		const float pcr[2] = { pcrL, pcrR };

		// Align dry to PC latency
		float sig[2];
		const int pcLat = (int)ch[0].latBuf.size(); // set in prepareToPlay

		for (int c = 0; c < 2; ++c) {
			auto& s = ch[c];

			sig[c]  = s.latBuf[(size_t)s.lpos];
			s.latBuf[(size_t)s.lpos] = data[c][n];
			s.lpos  = (s.lpos + 1) % pcLat;
		}

		// Stereo matrix
		float outL = sig[0] * gL  .getNextValue() + sig[1] * gRil.getNextValue();
		float outR = sig[1] * gR  .getNextValue() + sig[0] * gLir.getNextValue();

		outL += pce[0] * gPce[1].getNextValue() + pce[1] * gPce[0].getNextValue();
		outR += pce[1] * gPce[2].getNextValue() + pce[0] * gPce[3].getNextValue();

		outL += pcr[0] * gPcr[1].getNextValue() + pcr[1] * gPcr[0].getNextValue();
		outR += pcr[1] * gPcr[2].getNextValue() + pcr[0] * gPcr[3].getNextValue();

		// Delay -> phase -> separation
		float out[2] = { outL, outR };
		for (int c = 0; c < 2; ++c) {
			auto& s = ch[c];

			// Interpolating delay line
			const int sz = (int)s.dbuf.size();
			s.dbuf[(size_t)s.wpos] = out[c];

			float rp = (float)s.wpos - delaySmooth[c].getNextValue();

			while (rp < 0.0f) {
				rp += (float)sz;
			}

			const int   i0 = (int)rp % sz;
			const float f  = rp - (float)(int)rp;
			const int   i1 = (i0 + 1) % sz;
			const float x  = s.dbuf[(size_t)i0] + f * (s.dbuf[(size_t)i1] - s.dbuf[(size_t)i0]);

			s.wpos = (s.wpos + 1) % sz;

			// First-order all-pass: H(z) = (z ^ -1 - coeff)/(1 - coeff * z ^ -1), coeff = -cos(theta)
			const float coeff = -std::cos(phaseSmooth[c].getNextValue());
			const float y     = -coeff * x + s.apState;

			s.apState = x + coeff * y;
			out[c]    = y;
		}

		const float mid  = (out[0] + out[1]) * 0.5f;
		const float side = (out[0] - out[1]) * 0.5f * widthSmooth.getNextValue();

		const float widened[2] = {
			mid + side,
			mid - side
		};

		// Circular panning
		const float panN  = panSmooth.getNextValue(); // 0 = left, 0.5 = center, 1 = right
		const float theta = panN * juce::MathConstants<float>::halfPi;

		const float panL = std::sqrt(2.0f) * std::cos(theta);
		const float panR = std::sqrt(2.0f) * std::sin(theta);

		data[0][n] = widened[0] * panL;
		data[1][n] = widened[1] * panR;
	}
}

// -=-=-=- //
// Editor / state
// -=-=-=- //

juce::AudioProcessorEditor* AudioProcessor::createEditor() {
	return new AudioProcessorEditor(*this);
}

void AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
	auto state = apvts.copyState();
	state.setProperty("preset",    currentPreset,    nullptr);
	state.setProperty("uiScale",   savedUiScale,     nullptr);
	state.setProperty("pcFftSize", pcFftSize.load(), nullptr);

	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState && xmlState -> hasTagName(apvts.state.getType())) {
		apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

		currentPreset = juce::jlimit(
			0,
			(int)factoryPresets().size() - 1,
			(int)apvts.state.getProperty("preset", 0)
		);

		savedUiScale = juce::jlimit(
			0.75f, 2.0f,
			(float)apvts.state.getProperty("uiScale", 1.0f)
		);

		const int fftVal = (int)apvts.state.getProperty("pcFftSize", StereoMaskEngine::FFT);

		// Accept only powers of two in the supported range
		const bool isPow2 = fftVal > 0 && (fftVal & (fftVal - 1)) == 0;
		const bool inRange = fftVal >= 128 && fftVal <= 16384;

		pcFftSize.store((isPow2 && inRange) ? fftVal : StereoMaskEngine::FFT);
	}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
	return new AudioProcessor();
}