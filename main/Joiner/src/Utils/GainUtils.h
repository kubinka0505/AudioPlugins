#pragma once
/*
Bipolar fader math and display helpers
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace GainUtils {
    // t = 0.25 * 10 ^ (-0.2) ~= 0.1577...
    static const float kAnchorT = 0.25f * std::pow(10.0f, -0.2f);

    // log(10 ^ (-0.3)) / log(_ / 0.25) ~= 1.499...
    static const float kFaderPow = std::log(std::pow(10.0f, -0.3f)) / std::log(kAnchorT / 0.25f);

	// Positive-half mapping: t in [0, 0.5] -> linear gain
	inline float halfToGain(float t) noexcept {
		if (t <= 0.0f) {
			return 0.0f;
		}

		if (t < 0.25f) {
			return std::pow(t * 4.0f, kFaderPow);
		}

		return juce::Decibels::decibelsToGain(48.0f * (t - 0.25f));
	}

	inline float volToGain(float v) {
		if (std::abs(v - 0.5f) < 1e-6f) {
			return 0.0f;
		}

		return (v > 0.5f) ? halfToGain(v - 0.5f) : -halfToGain(0.5f - v);
	}

	// Full display string: polarity sign + 3-decimal dB value
	inline juce::String gainText(float v) {
		const float g  = volToGain(v);
		const float ag = std::abs(g);

		if (ag < 1e-6f) {
			return "-inf dB";
		}

		return juce::String(g < 0.0f ? "-" : "+") + juce::String(juce::Decibels::gainToDecibels(ag), 3) + " dB";
	}

	/*
	Parse a user-typed magnitude string back to a normalised [0,1] value.
	*/
	inline float gainFromText(const juce::String& magnitudeStr, bool positive) {
		const double dB = magnitudeStr.getDoubleValue(); // parses "6.000" -> 6.0

		if (!std::isfinite(dB)) {
			return 0.5f;
		}

		// Invert the two-segment curve to find t from dB
		float t = 0.0f;
		if (dB <= 0.0) {
			// Power-law segment
			const float gain = juce::Decibels::decibelsToGain((float)dB);
			t = 0.25f * std::pow(gain, 1.0f / kFaderPow);
		} else {
			// Linear-dB segment
			t = (float)dB / 48.0f + 0.25f;
		}

		t = juce::jlimit(0.0f, 0.5f, t);

		return positive ? (0.5f + t) : (0.5f - t);
	}

	// Display string for a bipolar knob (delay ms / phase deg / separation %)
	inline juce::String sideText(float v, float scale, const juce::String& unit) {
		if (std::abs(v - 0.5f) < 1e-4f) {
			return "0.000 " + unit;
		}

		const float a = std::abs(v - 0.5f) * 2.0f * scale;

		return juce::String(v < 0.5f ? "L " : "R ") + juce::String(a, 3) + " " + unit;
	}
}