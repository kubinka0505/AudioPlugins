#include "Editor.h"

// -=-=-=- //
// Constants
// -=-=-=- //

namespace {
	// Colors
	const juce::Colour kRed   { 0xFFDB0000 }; // main matrix + knobs
	const juce::Colour kGreen { 0xFF00FF00 }; // PCE / PCR

	// Snap points for the bipolar faders
	static const std::vector<double> kVolSnaps = [] {
		const double kPow = (double)GainUtils::kFaderPow;
		const double step = 20.0 * std::log10(2);

		const double t_6  = step / 48.0 + 1.0 / 4.0;
		const double t_12 = 0.25 * std::pow(1.0 / 4.0, 1.0 / kPow);
		const double t_m6 = 0.25 * std::pow(1.0 / 2.0, 1.0 / kPow);

		// Sorted ascending
		return std::vector<double> {
			1.0 / 2.0 - t_6,  // +6.021 dB, lower polarity
			1.0 / 4.0,        //  0.000 dB, lower polarity (inverted unity)
			1.0 / 2.0 - t_m6, // −6.021 dB, lower polarity
			1.0 / 2.0 - t_12, // −12.041 dB, lower polarity
			1.0 / 2.0,        //  −inf  (silence)
			1.0 / 2.0 + t_12, // −12.041 dB, upper polarity
			1.0 / 2.0 + t_m6, // −6.021 dB, upper polarity
			3.0 / 4.0,        //  0.000 dB, upper polarity (unity)
			1.0 / 2.0 + t_6,  // +6.021 dB, upper polarity
		};
	}();
	const std::vector<double> kKnobSnaps = { 1.0 / 2.0 };

	// Canvas geometry (base pixels, before scaling)
	constexpr int kFaderN    = 4;
	constexpr int kFaderW    = 66;                   // width of one fader cell
	constexpr int kSliderCol = kFaderW * kFaderN;    // left column width
	constexpr int kRightW    = 36 * kFaderN;         // right column width
	constexpr int kBaseW     = kSliderCol + kRightW;
	constexpr int kBaseH     = 700;

	constexpr int kTop       = 10;                   // space above the first fader row
	constexpr int kRowH      = (kBaseH - kTop) / 3;
	constexpr int kLabelH    = 18;
	constexpr int kBoxH      = 18;
	constexpr int kKnobSize  = 100;
	constexpr int kBoxW      = 118;                  // knob value box width

	//-=-=-=-//

	constexpr int kKnobCount         = 4;
	constexpr int kScaleRowH         = 28; // Scale label + combo row height
	constexpr int kScaleBottomPad    = 8;  // gap between Scale row and the first knob
	constexpr int kKnobTopPad        = 6;  // extra top padding for every knob after the first
	constexpr int kBottomGapAboveSep = 8;  // gap between last knob and the separator line
	constexpr int kBottomGapBelowSep = 8;  // gap between separator line and Invert/Swap
	constexpr int kButtonH           = 26;
	constexpr int kGapAfterButtons   = 8;
	constexpr int kFftRowH           = 20;

	constexpr int kScaleBlockH = kScaleRowH + kScaleBottomPad;
	constexpr int kBottomBlockH =
		kBottomGapAboveSep + kBottomGapBelowSep + kButtonH + kGapAfterButtons + kFftRowH;

	constexpr int kKnobsTop    = kScaleBlockH;
	constexpr int kKnobsAreaH  = kBaseH - kKnobsTop - kBottomBlockH;
	constexpr int kSectionH    = kKnobsAreaH / kKnobCount;
	constexpr int kKnobsBottom = kKnobsTop + kSectionH * kKnobCount;

	constexpr int kSepLineY = kKnobsBottom + kBottomGapAboveSep;
	constexpr int kButtonsY = kSepLineY    + kBottomGapBelowSep;
	constexpr int kFftY     = kButtonsY    + kButtonH + kGapAfterButtons;

	constexpr float kMinScale = 0.5f;
	constexpr float kMaxScale = 2.0f;

	// Fader rows: parameter prefix, label prefix, accent
	struct RowInfo {
		const char* idPrefix;
		const char* namePrefix;
		juce::Colour accent;
	};

	const RowInfo kRows[3] = {
		{ "",     "",     kRed   },
		{ "pce_", "PCE ", kGreen },
		{ "pcr_", "PCR ", kGreen },
	};

	// Fader columns: parameter id, label
	struct ColInfo {
		const char* id;
		const char* name;
	};
	const ColInfo kCols[4] = {
		{"ril", "RIL"},
		{"l", "L"},
		{"r", "R"},
		{"lir", "LIR"}
	};

	// Scale dropdown entries
	constexpr float kScales[]    = { kMinScale, 0.75f, 1.0f, 1.25f, 1.5f, kMaxScale };
	constexpr int   kScaleCount  = (int)(sizeof(kScales) / sizeof(kScales[0]));

	// Stereo mask FFT sizes: int values
	constexpr int kPcBufferSizes[] = { 2048, 4096, 8192, 16384 };
	constexpr int kPcBufferCount   = (int)(sizeof(kPcBufferSizes) / sizeof(kPcBufferSizes[0]));

	juce::String scaleLabel(float s) {
		return juce::String(juce::roundToInt(s * 100)) + "%";
	}

	void setParam(juce::RangedAudioParameter* p, float normalised) {
		p -> beginChangeGesture();
		p -> setValueNotifyingHost(normalised);
		p -> endChangeGesture();
	}
}

// -=-=-=- //
// Construction
// -=-=-=- //

AudioProcessorEditor::AudioProcessorEditor(AudioProcessor& p)
	: juce::AudioProcessorEditor(&p), audioProcessor(p)
{
	addAndMakeVisible(content);

	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 4; ++col) {
			addFader(row, col);
		}
	}

	// BPM button
	setupBpmButton();

	auto& apvts = audioProcessor.apvts;

	addKnob("Delay", "delay", std::make_unique<ValueMappings::Delay>([&apvts] {
		return apvts.getRawParameterValue("delay_bpm") -> load() > 0.5f;
	}));
	addKnob("Phase", "phase", std::make_unique<ValueMappings::Phase>());
	addKnob("Panning",   "pan",   std::make_unique<ValueMappings::Pan>());
	addKnob("Separation",   "sep",   std::make_unique<ValueMappings::Separation>());

	knobs[0] -> slider.quantise = [this] {
		return bpmButton.getToggleState();
	};

	bpmWatch = std::make_unique<juce::ParameterAttachment>(
		*apvts.getParameter("delay_bpm"),
		[this](float) {
			knobs[0] -> box -> refresh();
		}
	);

	setupInvertButton();
	setupSwapButton();
	setupScaleCombo();
	setupPcBufferCombo();

	// fixed aspect ratio
	constrainer.setFixedAspectRatio((double)kBaseW / (double)kBaseH);
	constrainer.setSizeLimits(
		juce::roundToInt(kBaseW * kMinScale), juce::roundToInt(kBaseH * kMinScale),
		juce::roundToInt(kBaseW * kMaxScale), juce::roundToInt(kBaseH * kMaxScale)
	);

	setConstrainer(&constrainer);
	setResizable(true, false); // host can still resize; no corner handle

	// restore previously saved UI scale
	const float restoredScale = audioProcessor.savedUiScale;

	setSize(
		juce::roundToInt(kBaseW * restoredScale),
		juce::roundToInt(kBaseH * restoredScale)
	);
}

AudioProcessorEditor::~AudioProcessorEditor() {
	bpmButton.setLookAndFeel(nullptr);
	invertButton.setLookAndFeel(nullptr);
	swapButton.setLookAndFeel(nullptr);
	scaleCombo.setLookAndFeel(nullptr);
	pcBufferCombo.setLookAndFeel(nullptr);
}

// -=-=-=- //
// Setup helpers
// -=-=-=- //

void AudioProcessorEditor::addFader(int row, int col) {
	const juce::String id   = juce::String(kRows[row].idPrefix) + kCols[col].id;
	const juce::String name = juce::String(kRows[row].namePrefix) + kCols[col].name;

	auto cell = std::make_unique<FaderCell>(kVolSnaps, kRows[row].accent);

	content.addAndMakeVisible(cell -> slider);

	cell -> label.setText(name, juce::dontSendNotification);
	cell -> label.setJustificationType(juce::Justification::centred);
	cell -> label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	content.addAndMakeVisible(cell -> label);

	cell -> attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, id, cell -> slider);

	cell -> box = std::make_unique<SplitValueBox>(
		*audioProcessor.apvts.getParameter(id),
		std::make_unique<ValueMappings::Fader>()
	);
	content.addAndMakeVisible(*cell -> box);

	faders.push_back(std::move(cell));
}

void AudioProcessorEditor::addKnob(
	const juce::String& name,
	const juce::String& paramID,
	std::unique_ptr<ValueMappings::Base> mapping
) {
	auto cell = std::make_unique<KnobCell>(kKnobSnaps, kRed);

	content.addAndMakeVisible(cell -> slider);

	cell -> label.setText(name, juce::dontSendNotification);
	cell -> label.setJustificationType(juce::Justification::centred);
	cell -> label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	content.addAndMakeVisible(cell -> label);

	cell -> attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, cell -> slider);

	cell -> box = std::make_unique<SplitValueBox>(
		*audioProcessor.apvts.getParameter(paramID),
		std::move(mapping)
	);
	content.addAndMakeVisible(*cell -> box);

	knobs.push_back(std::move(cell));
}

void AudioProcessorEditor::setupBpmButton() {
	bpmButton.setClickingTogglesState(true);
	bpmButton.setLookAndFeel(&utilityLaf);
	content.addAndMakeVisible(bpmButton);

	bpmAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "delay_bpm", bpmButton);

	// Switching mode keeps the delay time: convert the knob position to the other scale
	bpmButton.onClick = [this] {
		auto& apvts      = audioProcessor.apvts;
		const bool toBpm = bpmButton.getToggleState(); // already toggled when onClick fires

		double bpm = audioProcessor.getLastBpm();

		if (bpm <= 0.0) {
			bpm = 120.0;
		}

		setParam(apvts.getParameter("delay_bpm"), toBpm ? 1.0f : 0.0f);

		auto* delay      = apvts.getParameter("delay");
		const float v    = delay -> getValue();
		const float t    = std::abs(v - 0.5f) * 2.0f; // 0 at centre, 1 at extremes
		const float side = (v < 0.5f) ? -1.0f : 1.0f;
		const float maxS = DelayMap::kMaxMs / 1000.0f;

		float newT = 0.0f;

		if (toBpm) {
			// ms -> nearest note value
			const float beats = t * maxS * (float)bpm / 60.0f;
			newT = (float)DelayMap::stepForBeats(beats) / DelayMap::kSteps;
		} else {
			// note value -> ms (clamped to the 500 ms maximum)
			const int n = (int)std::lround(t * DelayMap::kSteps);

			if (n > 0) {
				newT = juce::jmin(1.0f, DelayMap::kBeats[n - 1] * 60.0f / (float)bpm / maxS);
			}
		}

		setParam(delay, 0.5f + side * newT * 0.5f);
	};
}

void AudioProcessorEditor::setupInvertButton() {
	invertButton.setLookAndFeel(&utilityLaf);
	content.addAndMakeVisible(invertButton);

	// Flip the polarity of every fader: v -> 1 - v keeps the dB magnitude
	invertButton.onClick = [this] {
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 4; ++col) {
				auto* p = audioProcessor.apvts.getParameter(
					juce::String(kRows[row].idPrefix) + kCols[col].id
				);
	
				setParam(p, 1.0f - p -> getValue());
			}
		}
	};
}

void AudioProcessorEditor::setupSwapButton() {
	swapButton.setLookAndFeel(&utilityLaf);
	content.addAndMakeVisible(swapButton);

	// In every row
	swapButton.onClick = [this] {
		auto& apvts   = audioProcessor.apvts;
		auto exchange = [&](const juce::String& a, const juce::String& b) {
			auto* pa = apvts.getParameter(a);
			auto* pb = apvts.getParameter(b);

			const float va = pa -> getValue();
			const float vb = pb -> getValue();

			setParam(pa, vb);
			setParam(pb, va);
		};

		for (const auto& row : kRows) {
			exchange(
				juce::String(row.idPrefix) + "l",
				juce::String(row.idPrefix) + "r"
			);

			exchange(
				juce::String(row.idPrefix) + "ril",
				juce::String(row.idPrefix) + "lir"
			);
		}
	};
}

void AudioProcessorEditor::setupScaleCombo() {
	scaleLabel.setText("Scale", juce::dontSendNotification);
	scaleLabel.setJustificationType(juce::Justification::centredLeft);
	scaleLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

	content.addAndMakeVisible(scaleLabel);

	for (int i = 0; i < kScaleCount; ++i) {
		scaleCombo.addItem(::scaleLabel(kScales[i]), i + 1);
	}

	scaleCombo.setLookAndFeel(&utilityLaf);
	content.addAndMakeVisible(scaleCombo);

	// Resizing the window is the single code path: resized() rescales the canvas
	scaleCombo.onChange = [this] {
		const int i = scaleCombo.getSelectedId() - 1;

		if (i >= 0 && i < kScaleCount) {
			setSize(
				juce::roundToInt(kBaseW * kScales[i]),
				juce::roundToInt(kBaseH * kScales[i])
			);
		}
	};
}

void AudioProcessorEditor::setupPcBufferCombo() {
	pcBufferLabel.setText("FFT", juce::dontSendNotification);
	pcBufferLabel.setJustificationType(juce::Justification::centredLeft);
	pcBufferLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	content.addAndMakeVisible(pcBufferLabel);

	for (int i = 0; i < kPcBufferCount; ++i) {
		pcBufferCombo.addItem(juce::String(kPcBufferSizes[i]), i + 1);
	}

	// Select the current size
	const int current = audioProcessor.pcFftSize.load();
	for (int i = 0; i < kPcBufferCount; ++i) {
		if (kPcBufferSizes[i] == current) {
			pcBufferCombo.setSelectedId(i + 1, juce::dontSendNotification);
			break;
		}
	}

	pcBufferCombo.setLookAndFeel(&utilityLaf);
	content.addAndMakeVisible(pcBufferCombo);

	pcBufferCombo.onChange = [this] {
		const int i = pcBufferCombo.getSelectedId() - 1;
		if (i < 0 || i >= kPcBufferCount) {
			return;
		}

		audioProcessor.pcFftSize.store(kPcBufferSizes[i]);
		audioProcessor.updateHostDisplay(
			juce::AudioProcessor::ChangeDetails().withLatencyChanged(true)
		);
	};
}

// -=-=-=- //
// Scaling
// -=-=-=- //

float AudioProcessorEditor::currentScale() const {
	return juce::jmin(
		(float)getWidth() / (float)kBaseW,
		(float)getHeight() / (float)kBaseH
	);
}

// Selects the dropdown entry matching the current window size (blank when dragged to a custom size).
// Also persists the current scale to the processor so it survives plugin re-focus / reload.
void AudioProcessorEditor::syncScaleCombo() {
	int id = 0;
	const float scale = currentScale();

	for (int i = 0; i < 5; ++i) {
		if (std::abs(scale - kScales[i]) < 0.01f) {
			id = i + 1;
		}
	}

	scaleCombo.setSelectedId(id, juce::dontSendNotification);

	// Always persist whatever the actual scale is (even a custom host-driven size)
	audioProcessor.savedUiScale = scale;
}

// -=-=-=- //
// Paint
// -=-=-=- //

void AudioProcessorEditor::paint(juce::Graphics& g) {
	g.fillAll(juce::Colours::black);

	// Dividers are drawn in canvas coordinates
	g.addTransform(juce::AffineTransform::scale(currentScale()));
	g.setColour(juce::Colour(0xFF333333));

	g.drawHorizontalLine(kTop + kRowH,     0.0f, (float)kSliderCol);
	g.drawHorizontalLine(kTop + kRowH * 2, 0.0f, (float)kSliderCol);
	g.drawVerticalLine(kSliderCol, 0.0f, (float)kBaseH);

	// Right column: separator directly above Invert / Swap / FFT dropdown
	g.drawHorizontalLine(kSepLineY, (float)kSliderCol, (float)kBaseW);
}

// -=-=-=- //
// Layout (in canvas coordinates)
// -=-=-=- //

void AudioProcessorEditor::resized() {
	// Can be called by JUCE before the constructor has created every control
	if (faders.size() < 12 || knobs.size() < 4) {
		return;
	}

	content.setBounds(0, 0, kBaseW, kBaseH);
	content.setTransform(juce::AffineTransform::scale(currentScale()));
	syncScaleCombo();

	// Fader rows

	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 4; ++col) {
			auto& f = *faders[(size_t)(row * 4 + col)];

			// Full row cell
			auto cell = juce::Rectangle<int>(col * kFaderW, kTop + row * kRowH, kFaderW, kRowH);

			// Symmetric inner padding
			cell.removeFromTop(kTop);
			cell.removeFromBottom(kTop);

			f.label.setBounds(cell.removeFromTop(kLabelH));
			f.box -> setBounds(cell.removeFromBottom(kBoxH).reduced(1, 0));
			f.slider.setBounds(cell.reduced(3, 6)); // half of thumbD (see LookAndFeel)
		}
	}

	// Right column
	constexpr int gap = 8;

	// Scale row
	const int rowY   = (kScaleRowH - 20) / 2;
	const int labelW = 38;

	const int comboX = kSliderCol + gap + labelW + 4;
	const int comboW = kBaseW - comboX - gap;

	scaleLabel.setBounds(kSliderCol + gap, rowY, labelW, 20);
	scaleCombo.setBounds(comboX,           rowY, comboW, 20);

	// Knobs
	for (int i = 0; i < kKnobCount; ++i) {
		auto& k        = *knobs[(size_t)i];
		const int yTop = kKnobsTop + i * kSectionH;
		auto area      = juce::Rectangle<int>(kSliderCol, yTop, kRightW, kSectionH);

		if (i > 0) {
			area.removeFromTop(kKnobTopPad);
		}

		// Delay label (index 0) is trimmed on the right for the BPM button
		k.label.setBounds(area.removeFromTop(kLabelH).withTrimmedRight(i == 0 ? 38 : 0));

		auto boxRow = area.removeFromBottom(kBoxH + 4);
		k.box -> setBounds(boxRow.withSizeKeepingCentre(kBoxW, kBoxH));

		k.slider.setBounds(area.withSizeKeepingCentre(kKnobSize, kKnobSize));
	}

	// BPM button
	bpmButton.setBounds(kSliderCol + kRightW - 38, kKnobsTop + 1, 36, kLabelH - 2);

	// Bottom area
	const int btnW = (kRightW - gap * 3) / 2;

	invertButton.setBounds(kSliderCol + gap,            kButtonsY, btnW, kButtonH);
	swapButton  .setBounds(kSliderCol + gap * 2 + btnW, kButtonsY, btnW, kButtonH);

	const int pcLblW = 26; // "FFT" at normal size
	const int pcCmbX = kSliderCol + gap + pcLblW + 4;
	const int pcCmbW = kBaseW - pcCmbX - gap;

	pcBufferLabel.setBounds(kSliderCol + gap, kFftY, pcLblW, kFftRowH);
	pcBufferCombo.setBounds(pcCmbX,           kFftY, pcCmbW, kFftRowH);
}