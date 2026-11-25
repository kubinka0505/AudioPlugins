#pragma once
/*
Built-in factory presets shown by the host as the plugin's program list
*/

#include <cmath>
#include <utility>
#include <vector>

struct Preset {
	const char* name;
	std::vector<std::pair<const char*, float>> values;
};

namespace detail {
	// 10 ^ (dB/20) — converts a gain in dB to a linear amplitude
	inline float dbToLin(float db) {
		return std::pow(10.0f, db / 20.0f);
	}

	// The "m/s" split magnitude used by the M/S presets.
	// 0.657732... ~= -3.63647 dB, and 0.342265... = 1 - that.
	constexpr float kMSDb = -3.63647f;

	// 0.657732439041138
	inline float msHi() {
		return dbToLin(kMSDb);
	}

	// 0.342265624552965
	inline float msLo() {
		return 1.0f - msHi();
	}
}

inline const std::vector<Preset>& factoryPresets() {
	using namespace detail;

	static const std::vector<Preset> presets = {
		{ "Default", {
			{"ril", 1.f/2},
			{"l",   3.f/4},
			{"r",   3.f/4},
			{"lir", 1.f/2},
		}},

		{ "Swap Channels", {
			{"ril", 3.f/4},
			{"l",   1.f/2},
			{"r",   1.f/2},
			{"lir", 3.f/4},
		}},

		{ "Invert", {
			{"ril", 1.f/2},
			{"l",   1.f/4},
			{"r",   1.f/4},
			{"lir", 1.f/2},
		}},

		{ "Invert (Left)", {
			{"ril", 1.f/2},
			{"l",   1.f/4},
			{"r",   3.f/4},
			{"lir", 1.f/2},
		}},

		{ "Invert (Right)", {
			{"ril", 1.f/2},
			{"l",   3.f/4},
			{"r",   1.f/4},
			{"lir", 1.f/2},
		}},

		{ "Solo (Left)", {
			{"ril", 1.f/2},
			{"l",   3.f/4},
			{"r",   1.f/2},
			{"lir", 1.f/2},
		}},

		{ "Solo (Right)", {
			{"ril", 1.f/2},
			{"l",   1.f/2},
			{"r",   3.f/4},
			{"lir", 1.f/2},
		}},

		{ "Mid (Mono)", {
			{"ril", msHi()},
			{"l",   msHi()},
			{"r",   msHi()},
			{"lir", msHi()},
		}},

		{ "Mid (Left)", {
			{"ril", 1.f/2},
			{"l",   3.f/4},
			{"r",   1.f/2},
			{"lir", 3.f/4},
		}},

		{ "Mid (Right)", {
			{"ril", 3.f/4},
			{"l",   1.f/2},
			{"r",   3.f/4},
			{"lir", 1.f/2},
		}},

		{ "Side", {
			{"ril", msLo()},
			{"l",   msHi()},
			{"r",   msHi()},
			{"lir", msLo()},
		}},

		{ "Side (Mono)", {
			{"ril", msLo()},
			{"l",   msHi()},
			{"r",   msLo()},
			{"lir", msHi()},
		}},

		{ "L/R = M/S", {
			{"ril", 3.f/4},
			{"l",   3.f/4},
			{"r",   msLo()},
			{"lir", msHi()},
		}},

		{ "Center", {
			{"ril", 1.f/2},
			{"l",   1.f/2},
			{"r",   1.f/2},
			{"lir", 1.f/2},

			{"pce_ril", 1.f/2},
			{"pce_l",   3.f/4},
			{"pce_r",   3.f/4},
			{"pce_lir", 1.f/2},
		}},

		{ "Sides", {
			{"ril", 1.f/2},
			{"l",   1.f/2},
			{"r",   1.f/2},
			{"lir", 1.f/2},

			{"pcr_ril", 1.f/2},
			{"pcr_l",   3.f/4},
			{"pcr_r",   3.f/4},
			{"pcr_lir", 1.f/2},
		}},

		{ "Original - Mask", {
			{"ril", 1.f/2},
			{"l",   1.f/4},
			{"r",   1.f/4},
			{"lir", 1.f/2},

			{"pce_ril", 1.f/2},
			{"pce_l",   3.f/4},
			{"pce_r",   3.f/4},
			{"pce_lir", 1.f/2},

			{"pcr_ril", 1.f/2},
			{"pcr_l",   3.f/4},
			{"pcr_r",   3.f/4},
			{"pcr_lir", 1.f/2},
		}},
	};

	return presets;
}