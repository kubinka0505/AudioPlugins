#pragma once
/*
Reusable GUI widgets
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <vector>
#include <cmath>
#include "ValueMappings.h"

/*
Look and feel built from a single accent colour.

JUCE's LookAndFeel_V4 paints a fader's filled part with trackColourId, the rail with
backgroundColourId and the thumb with thumbColourId, so all of them are set here.
*/
class LookAndFeel : public juce::LookAndFeel_V4 {
	public:
		explicit LookAndFeel(juce::Colour accentColour) : accent(accentColour) {
			setColour(juce::Slider::trackColourId,               accent);
			setColour(juce::Slider::thumbColourId,               accent);
			setColour(juce::Slider::backgroundColourId,          accent.darker(0.9f));
			setColour(juce::Slider::rotarySliderFillColourId,    accent);
			setColour(juce::Slider::rotarySliderOutlineColourId, accent.darker(0.7f));
		}

		void drawLinearSlider(
			juce::Graphics& g,

			int x,
			int y,

			int w,
			int h,

			float sliderPos,

			float minSliderPos,
			float maxSliderPos,

			juce::Slider::SliderStyle style,
			juce::Slider& slider
		) override {
			if (style != juce::Slider::LinearVertical) {
				LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, sliderPos, minSliderPos, maxSliderPos, style, slider);
				return;
			}

			constexpr float trackW = 6.0f;  // wider than the default ~4 px
			constexpr float thumbD = 14.0f; // circle diameter

			const float cx   = (float)x + (float)w * 0.5f;
			const float top  = (float)y;
			const float bot  = (float)(y + h);
			const float mid  = (float)y + (float)h * 0.5f; // centre = silence (0.5 normalised)

			// Background rail
			g.setColour(
				findColour(juce::Slider::backgroundColourId)
			);
			g.fillRoundedRectangle(
				cx - trackW * 0.5f,
				top,
				trackW,
				bot - top,
				trackW * 0.5f
			);

			// Filled portion between centre (silence) and thumb
			const float fillTop = std::min(sliderPos, mid);
			const float fillBot = std::max(sliderPos, mid);

			g.setColour(
				findColour(juce::Slider::trackColourId)
			);
			g.fillRoundedRectangle(
				cx - trackW * 0.5f,
				fillTop,
				trackW,
				fillBot - fillTop,
				trackW * 0.5f
			);

			// Thumb - 20 px circle
			g.setColour(
				findColour(juce::Slider::thumbColourId).brighter(0.3f)
			);
			g.fillEllipse(
				cx - thumbD * 0.5f,
				sliderPos - thumbD * 0.5f,
				thumbD,
				thumbD
			);
		}

	private:
		juce::Colour accent;
};

// -=-=-=- //

// Shared look-and-feel
class UtilityLookAndFeel : public juce::LookAndFeel_V4 {
	public:
		static constexpr float kCornerRadius = 2.0f;

		UtilityLookAndFeel() {
			const auto bg = juce::Colour(0xFF222222);

			setColour(juce::TextButton::buttonColourId,   bg);
			setColour(juce::TextButton::buttonOnColourId, bg.brighter(0.25f));
			setColour(juce::TextButton::textColourOffId,  juce::Colours::lightgrey);
			setColour(juce::TextButton::textColourOnId,   juce::Colours::white);

			setColour(juce::ComboBox::backgroundColourId, bg);
			setColour(juce::ComboBox::outlineColourId,    bg.brighter(2.0f / 3));
			setColour(juce::ComboBox::textColourId,       juce::Colours::lightgrey);
			setColour(juce::ComboBox::arrowColourId,      juce::Colours::lightgrey);
		}

		void drawButtonBackground(
			juce::Graphics& g,
			juce::Button& button,

			const juce::Colour& backgroundColour,

			bool isHighlighted,
			bool isDown
		) override {
			auto bounds = button.getLocalBounds().toFloat();

			auto fill = backgroundColour;
			if (isDown) {
				fill = fill.brighter(0.25f);
			} else if (isHighlighted) {
				fill = fill.brighter(0.12f);
			}

			g.setColour(fill);
			g.fillRoundedRectangle(bounds, kCornerRadius);
		}

		void drawComboBox(
			juce::Graphics& g,

			int width,
			int height,

			bool /*isButtonDown*/,

			int /*buttonX*/,
			int /*buttonY*/,
			int /*buttonW*/,
			int /*buttonH*/,

			juce::ComboBox& box
		) override {
			auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);

			g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
			g.fillRoundedRectangle(bounds, kCornerRadius);

			g.setColour(box.findColour(juce::ComboBox::outlineColourId));
			g.drawRoundedRectangle(bounds.reduced(0.5f), kCornerRadius, 1.0f);

			// Small downward-pointing arrow on the right
			const auto arrowZone = juce::Rectangle<float>((float)width - 18.0f, 0.0f, 18.0f, (float)height);
			juce::Path arrow;
			arrow.startNewSubPath(arrowZone.getX() + 4.0f, arrowZone.getCentreY() - 2.0f);
			arrow.lineTo(arrowZone.getCentreX(),           arrowZone.getCentreY() + 3.0f);
			arrow.lineTo(arrowZone.getRight() - 4.0f,      arrowZone.getCentreY() - 2.0f);

			g.setColour(box.findColour(juce::ComboBox::arrowColourId));
			g.strokePath(arrow, juce::PathStrokeType(1.5f));
		}
};

// -=-=-=- //

/*
Slider with:
- normalised snap points (hold Ctrl to move freely),
- optional BPM-step quantising via the `quantise` callback,
- an accent colour (fill, rail, thumb / knob body).
*/
class SnapSlider : public juce::Slider {
	public:
		SnapSlider(
			juce::Slider::SliderStyle style,
			const std::vector<double>& snapPoints,

			juce::Colour accent = juce::Colours::white,
			double snapRadius   = 0.03
		) : juce::Slider(style, juce::Slider::NoTextBox),
			snaps(snapPoints), radius(snapRadius), laf(accent)
		{
			setLookAndFeel(&laf);
		}

		~SnapSlider() override { setLookAndFeel(nullptr); }

		// Return true to quantise to the BPM-mode delay note positions
		std::function<bool()> quantise;

		double snapValue(double v, DragMode) override {
			if (juce::ModifierKeys::currentModifiers.isCtrlDown()) {
				return v;
			}

			if (quantise && quantise()) {
				const double steps = 2.0 * DelayMap::kSteps;
				return 0.5 + std::round((v - 0.5) * steps) / steps;
			}

			for (double s : snaps) {
				if (std::abs(v - s) < radius) {
					return s;
				}
			}

			return v;
		}

	private:
		std::vector<double> snaps;
		double radius;
		LookAndFeel laf;
};

// -=-=-=- //

class SplitValueBox : public juce::Component {
	public:
		SplitValueBox(
			juce::RangedAudioParameter& parameter,
			std::unique_ptr<ValueMappings::Base> valueMapping
		): param(parameter),
			mapping(std::move(valueMapping)),
			attachment(parameter, [this](float) {
				refresh();
			})
		{
			const juce::Font font {
				juce::FontOptions(10.5f)
			};

			prefixCell.setFont(font, juce::Justification::centred);
			prefixCell.setMouseCursor(juce::MouseCursor::PointingHandCursor);
			prefixCell.onClick = [this] {
				togglePrefix();
			};

			unitCell.setFont(font, juce::Justification::centredLeft);

			editor.setFont(font);
			editor.setBorder(juce::BorderSize<int>(0));
			editor.setIndents(2, 0);

			editor.setJustification(juce::Justification::centred);
			editor.setSelectAllWhenFocused(true);

			editor.setInputRestrictions(12, "0123456789.-+/infINF");

			editor.setColour(
				juce::TextEditor::backgroundColourId,
				juce::Colour(0xFF161616)
			);
			editor.setColour(
				juce::TextEditor::textColourId,
				juce::Colours::white
			);
			editor.setColour(
				juce::TextEditor::outlineColourId,
				juce::Colour(0xFF444444)
			);
			editor.setColour(
				juce::TextEditor::focusedOutlineColourId,
				juce::Colours::white.withAlpha(0.6f)
			);

			// -=-=-=- //

			editor.onReturnKey = [this] {
				commit();
			};
			editor.onFocusLost = [this] {
				commit();
			};
			editor.onEscapeKey = [this] {
				cancel();
			};

			// -=-=-=- //

			if (mapping -> hasPrefix()) {
				addAndMakeVisible(prefixCell);
			}

			addAndMakeVisible(editor);
			addAndMakeVisible(unitCell);

			attachment.sendInitialUpdate();
		}

		// Re-read the parameter and redraw prefix, number and unit
		void refresh(bool force = false) {
			const float v = param.getValue();

			if (v > 0.5f) {
				upper = true;
			} else if (v < 0.5f) {
				upper = false;
			}

			prefixCell.setText(mapping -> prefix(upper));
			unitCell.setText(mapping -> unit());

			shown = mapping -> number(v);

			if (force || !editor.hasKeyboardFocus(false)) {
				editor.setText(shown, false);
			}

			resized();
		}

		void resized() override {
			auto area = getLocalBounds();

			if (mapping -> hasPrefix()) {
				prefixCell.setBounds(area.removeFromLeft(12));
			}

			const juce::Font font { juce::FontOptions(10.5f) };
			const int unitW = juce::roundToInt(juce::GlyphArrangement::getStringWidth(font, unitCell.getText())) + 4 + 3;

			unitCell.setBounds(area.removeFromRight(unitW));
			editor.setBounds(area);
		}

	private:
		// Tiny text cell
		class TextCell : public juce::Component {
			public:
				std::function<void()> onClick;

				void setFont(const juce::Font& f, juce::Justification j) {
					font = f; justification = j;
				}

				void setText(const juce::String& t) {
					text = t; repaint();
				}

				const juce::String& getText() const {
					return text;
				}

				// -=-=-=- //

				void paint(juce::Graphics& g) override {
					g.setColour(juce::Colours::white.withAlpha(0.85f));
					g.setFont(font);
					g.drawText(text, getLocalBounds(), justification);
				}

				void mouseUp(const juce::MouseEvent& e) override {
					if (onClick && e.mouseWasClicked()) {
						onClick();
					}
				}

			private:
				juce::Font  font;
				juce::Justification justification {
					juce::Justification::centred
				};
				juce::String text;
		};

		void commit() {
			if (committing) {
				return;
			}

			committing = true;

			const juce::String text = editor.getText().trim();

			if (text != shown) {
				const float newValue = mapping -> fromNumber(text, upper, param.getValue());
				attachment.setValueAsCompleteGesture(juce::jlimit(0.0f, 1.0f, newValue));
			}

			editor.giveAwayKeyboardFocus();
			committing = false;

			refresh(true);
		}

		void cancel() {
			committing = true;
			editor.giveAwayKeyboardFocus();
			committing = false;

			refresh(true);
		}

		void togglePrefix() {
			const float v = param.getValue();
			upper = !upper;

			if (std::abs(v - 0.5f) > 1e-6f) {
				attachment.setValueAsCompleteGesture(1.0f - v);
			}

			refresh();
		}

		juce::RangedAudioParameter& param;
		std::unique_ptr<ValueMappings::Base> mapping;

		TextCell prefixCell, unitCell;
		juce::TextEditor editor;

		juce::String shown;
		bool upper = true;
		bool committing = false;

		// Declared last: its callback needs every member above to exist
		juce::ParameterAttachment attachment;
};