#pragma once
/*
Editor declaration
------------------
All controls live on a fixed-size canvas

The canvas is scaled to fill the editor window, so
the window can be any size with the same aspect ratio:
- drag the resize handle in the bottom-right corner, or
- pick a size in the Scale dropdown.
*/

#include "Processor.h"
#include "../Utils/UIComponents.h"

// -=-=-=- //

class AudioProcessorEditor : public juce::AudioProcessorEditor {
	public:
		AudioProcessorEditor(AudioProcessor&);
		~AudioProcessorEditor() override; // detaches utilityLaf from every button/combo

		void paint(juce::Graphics&) override;
		void resized() override;

	private:
		using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
		using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

		// One fader: slider + name label + inline value box
		struct FaderCell {
			FaderCell(const std::vector<double>& snaps, juce::Colour accent)
				: slider(juce::Slider::LinearVertical, snaps, accent) {}

			SnapSlider                     slider;
			juce::Label                    label;

			std::unique_ptr<SplitValueBox> box;
			std::unique_ptr<SliderAttachment> attach;
		};

		// One knob: same parts as a fader
		struct KnobCell {
			KnobCell(const std::vector<double>& snaps, juce::Colour accent)
				: slider(juce::Slider::RotaryHorizontalVerticalDrag, snaps, accent) {}

			SnapSlider  slider;
			juce::Label label;

			std::unique_ptr<SplitValueBox> box;
			std::unique_ptr<SliderAttachment> attach;
		};

		// Setup
		void addFader(int row, int col);

		void addKnob(
			const juce::String& name,
			const juce::String& paramID,

			std::unique_ptr<ValueMappings::Base> mapping
		);

		void setupBpmButton();
		void setupInvertButton();
		void setupSwapButton();
		void setupScaleCombo();
		void setupPcBufferCombo();

		// Scaling
		float currentScale() const;
		void  syncScaleCombo();

		AudioProcessor& audioProcessor;

		// Shared by every TextButton / ComboBox
		UtilityLookAndFeel utilityLaf;

		// Fixed-size canvas holding every control (scaled to the window in resized())
		juce::Component content;

		// 12 faders, index = row * 4 + col
		std::vector<std::unique_ptr<FaderCell>> faders;

		// delay, phase, separation
		std::vector<std::unique_ptr<KnobCell>>  knobs;

		juce::TextButton bpmButton    { "BPM"    };
		juce::TextButton invertButton { "Invert" };
		juce::TextButton swapButton   { "Swap"   };

		juce::Label    scaleLabel;
		juce::ComboBox scaleCombo;

		juce::Label    pcBufferLabel;
		juce::ComboBox pcBufferCombo;

		std::unique_ptr<ButtonAttachment> bpmAttach;

		// refreshes the delay box when BPM mode changes
		std::unique_ptr<juce::ParameterAttachment> bpmWatch;

		juce::ComponentBoundsConstrainer constrainer;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioProcessorEditor)
};