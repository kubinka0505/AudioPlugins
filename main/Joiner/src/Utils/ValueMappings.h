#pragma once
/*
How a parameter is split into [prefix][number][unit] and parsed back
*/

#include <juce_core/juce_core.h>
#include <functional>
#include "GainUtils.h"
#include "DelayUtils.h"

namespace ValueMappings {
	class Base {
		public:
			virtual ~Base() = default;

			virtual bool hasPrefix() const {
				return true;
			}

			virtual juce::String unit() const = 0;
			virtual juce::String prefix(bool upper) const = 0;
			virtual juce::String number(float v) const = 0;

			virtual float fromNumber(
				const juce::String& text,
				bool upper,
				float current
			) const = 0;

		protected:
			static bool hasDigit(const juce::String& s) {
				return s.containsAnyOf("0123456789");
			}

			static float fromMagnitude(float t, bool upper) {
				return 0.5f + (upper ? 0.5f : -0.5f) * juce::jlimit(0.0f, 1.0f, t);
			}
	};

	// -=-=-=- //

	// Bipolar fader
	class Fader : public Base {
		public:
			juce::String unit() const override {
				return "dB";
			}

			juce::String prefix(bool upper) const override {
				return upper ? "+" : "-";
			}

			juce::String number(float v) const override {
				const float g = std::abs(GainUtils::volToGain(v));

				if (g < 1e-6f) {
					return "-inf";
				}

				return juce::String(juce::Decibels::gainToDecibels(g), 3);
			}

			float fromNumber(const juce::String& text, bool upper, float current) const override {
				if (text.containsIgnoreCase("inf")) {
					return 0.5f;
				}

				if (!hasDigit(text)) {
					return current;
				}

				return GainUtils::gainFromText(
					juce::String(text.getDoubleValue()),
					upper
				);
			}
	};

	// -=-=-=- //

	// Knob: delay
	class Delay : public Base {
		public:
			explicit Delay(std::function<bool()> bpmModeGetter) : bpmMode(std::move(bpmModeGetter)) {}

			juce::String unit() const override {
				return bpmMode() ? "beat" : "ms";
			}

			juce::String prefix(bool upper) const override {
				return upper ? "R" : "L";
			}

			juce::String number(float v) const override {
				const float t = std::abs(v - 0.5f) * 2.0f;

				if (!bpmMode()) {
					return juce::String(t * DelayMap::kMaxMs, 3);
				}

				const int n = (int)std::lround(t * DelayMap::kSteps);

				return n <= 0 ? juce::String("0") : juce::String(DelayMap::kNames[n - 1]);
			}

			float fromNumber(const juce::String& text, bool upper, float current) const override {
				if (!hasDigit(text)) {
					return current;
				}

				if (!bpmMode()) {
					return fromMagnitude((float)std::abs(text.getDoubleValue()) / DelayMap::kMaxMs, upper);
				}

				constexpr auto sep = "/";

				// "1/8" or a plain number of beats
				double beats = text.getDoubleValue();

				if (text.contains(sep)) {
					const double num = text.upToFirstOccurrenceOf(sep, false, false).getDoubleValue();
					const double den = text.fromFirstOccurrenceOf(sep, false, false).getDoubleValue();

					beats = den > 0.0 ? num / den : 0.0;
				}

				const int n = DelayMap::stepForBeats((float)std::abs(beats));

				return fromMagnitude((float)n / DelayMap::kSteps, upper);
			}

		private:
			std::function<bool()> bpmMode;
	};

	// -=-=-=- //

	// Knob: phase
	class Phase : public Base {
		public:
			juce::String unit() const override {
				return juce::String::fromUTF8("\xc2\xb0");
			}

			juce::String prefix(bool upper) const override {
				return upper ? "R" : "L";
			}

			juce::String number(float v) const override {
				return juce::String(std::abs(v - 0.5f) * 2.0f * 180.0f, 3);
			}

			float fromNumber(const juce::String& text, bool upper, float current) const override {
				if (!hasDigit(text)) {
					return current;
				}

				return fromMagnitude(
					(float)std::abs(text.getDoubleValue()) / 180.0f,
					upper
				);
			}
	};

	// -=-=-=- //

	// Knob: separation
	class Separation : public Base {
		public:
			bool hasPrefix() const override {
				return false;
			}

			juce::String unit() const override {
				return "%";
			}

			juce::String prefix(bool) const override {
				return {};
			}

			juce::String number(float v) const override {
				return juce::String(v * 200.0f, 3);
			}

			float fromNumber(const juce::String& text, bool, float current) const override {
				if (!hasDigit(text)) {
					return current;
				}

				return juce::jlimit(0.0f, 1.0f, (float)std::abs(text.getDoubleValue()) / 200.0f);
			}
	};
}