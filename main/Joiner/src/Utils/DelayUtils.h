#pragma once
/*
Delay knob mapping
------------------
Delay knob value v in [0, 1], 0.5 = no delay, each side is one channel:
- v < 0.5  ->  left channel delayed
- v > 0.5  ->  right channel delayed

BPM off:
- Linear 0 - 500 ms per side.
BPM on:
 - Multiple discrete note values per side.
*/

#include <cmath>

namespace DelayMap {
	inline constexpr float kMaxMs = 500.0f;

	inline constexpr float kBeats[] = {
		1.0f / 256,
		1.0f / 128,
		1.0f / 64,
		1.0f / 32,
		1.0f / 16,
		1.0f / 8,
		1.0f / 4,
		1.0f / 2,
		1.0f,
		2.0f,
		4.0f,
		8.0f
	};

	inline constexpr int kSteps = static_cast<int>(sizeof(kBeats) / sizeof(kBeats[0]));

	// Display names
	inline constexpr const char* kNames[kSteps] = {
		"1/256",
		"1/128",
		"1/64",
		"1/32",
		"1/16",
		"1/8",
		"1/4",
		"1/2",
		"1",
		"2",
		"4",
		"8"
	};

	// Delay duration in seconds for knob value v.
	inline float seconds(float v, bool bpmMode, double bpm) {
		const float t = std::abs(v - 0.5f) * 2.0f;

		if (!bpmMode) {
			return t * (kMaxMs / 1000.0f);
		}

		const int n = (int)std::lround(t * kSteps);

		if (n <= 0) {
			return 0.0f;
		}

		return kBeats[n - 1] * 60.0f / (float)(bpm > 0.0 ? bpm : 120.0);
	}

	// Nearest note step (1..kSteps, log distance) for a length in beats, 0 = "no delay".
	inline int stepForBeats(float beats) {
		if (beats < kBeats[0] * 0.5f) {
			return 0;
		}

		int best = 0;

		for (int i = 1; i < kSteps; ++i) {
			if (std::abs(std::log(beats / kBeats[i])) < std::abs(std::log(beats / kBeats[best]))) {
				best = i;
			}
		}

		return best + 1;
	}
}