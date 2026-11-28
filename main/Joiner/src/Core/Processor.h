#pragma once
/*
Processor declaration
---------------------
Signal chain per sample:
-> Input
-> StereoMaskEngine (split to PCE mono + PCR stereo, LATENCY samples delay)
-> Align dry input to PC latency (fixed ring buffer)
-> Stereo matrix: main (RIL/L/R/LIR) + PCE rows + PCR rows
-> Variable delay (ms or BPM-synced note, up to 10 s)
-> First-order all-pass phase shifter
-> Mid/Side width control
-> Circular panning
-> Output
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include <array>
#include "StereoMask.h"
#include "../Utils/GainUtils.h"
#include "../Utils/DelayUtils.h"
#include "../Presets.h"

// -=-=-=- //

class AudioProcessor : public juce::AudioProcessor {
public:
	AudioProcessor();
	~AudioProcessor() override;

	void prepareToPlay(double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;
	void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	juce::AudioProcessorEditor* createEditor() override;
	bool hasEditor() const override { return true; }

	const juce::String getName() const override { return "Joiner"; }
	bool acceptsMidi() const override { return false; }
	bool producesMidi() const override { return false; }
	double getTailLengthSeconds() const override { return 0.5; }

	// Sidechain bus layout
	bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

	// UI scale persistence (not an audio parameter, stored in state XML)
	float savedUiScale = 1.0f;

	// Desired stereo mask FFT size
	// processBlock() detects when this differs from activeFftSize and rebuilds.
	std::atomic<int> pcFftSize    { StereoMaskEngine::FFT };

	// FFT size the engine is currently running at (audio-thread owned, read-only for UI).
	std::atomic<int> activeFftSize{ StereoMaskEngine::FFT };

	// -=-=-=- //

	// Preset support (VST3 program list)
	int  getNumPrograms()    override { return (int)factoryPresets().size(); }
	int  getCurrentProgram() override { return currentPreset; }

	void setCurrentProgram(int index) override;
	const juce::String getProgramName(int index) override;

	void changeProgramName(int, const juce::String&) override {}

	void getStateInformation(juce::MemoryBlock& destData) override;
	void setStateInformation(const void* data, int sizeInBytes) override;

	// -=-=-=- //

	// Public API
	juce::AudioProcessorValueTreeState apvts;

	// Last BPM received from host play-head (120 if none yet).
	double getLastBpm() const { return lastBpm.load(); }

	// Formatted display text for the Delay knob (respects BPM mode).
	juce::String delayText(float v) const;

	// Fills out[0] (L delay, samples) and out[1] (R delay, samples) for knob value v.
	void delayTargets(float v, float out[2]) const;

private:
	juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

	// Per-channel DSP state
	struct Channel {
		// All-pass filter state
		float              apState = 0.0f;

		// Fixed ring: aligns dry to PC latency
		std::vector<float> latBuf;
		int                lpos = 0;

		// Variable delay line
		std::vector<float> dbuf;
		int                wpos = 0;
	};
	Channel ch[2];

	// Gain smoothers (20 ms ramp)
	juce::SmoothedValue<float> gLir, gL, gR, gRil;

	juce::SmoothedValue<float> delaySmooth[2];
	juce::SmoothedValue<float> phaseSmooth[2];
	juce::SmoothedValue<float> widthSmooth;
	juce::SmoothedValue<float> panSmooth;

	// [RIL, L, R, LIR]
	juce::SmoothedValue<float> gPce[4], gPcr[4];

	StereoMaskEngine pc;

	double currentSampleRate = 44100.0;
	int    maxDelaySamples   = 0;

	std::atomic<double> lastBpm       { 120.0 };
	std::atomic<float>* delayBpmParam = nullptr;

	// Presets
	int currentPreset = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioProcessor)
};