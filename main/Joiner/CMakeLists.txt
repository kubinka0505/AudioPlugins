cmake_minimum_required(VERSION 3.22)
project(Joiner VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
	JUCE
	GIT_REPOSITORY https://github.com/juce-framework/JUCE
	GIT_TAG		  8.0.4
	GIT_SHALLOW	 TRUE
)
FetchContent_MakeAvailable(JUCE)

if(NOT COMMAND juce_add_plugin)
	message(FATAL_ERROR "JUCE not fetched correctly.")
endif()

set(PLUGIN_TARGET Joiner)

if(UNIX AND NOT APPLE)
	 set(PLUGIN_FORMATS VST3)
else()
	 set(PLUGIN_FORMATS VST3 Unity ARA AAX)
endif()

juce_add_plugin(${PLUGIN_TARGET}
	PRODUCT_NAME			"Joiner"
	PLUGIN_CODE			Jner

	COMPANY_NAME			"kubinka0505"
	COMPANY_EMAIL			"kubinka0505@yandex.com"
	COMPANY_WEBSITE			"https://kubinka0505.github.io"
	PLUGIN_MANUFACTURER_CODE	KU05

	PLUGIN_CHARACTERISTICS		Effect
	VST3_CATEGORIES	"Stereo"

	IS_SYNTH			FALSE
	NEEDS_MIDI_INPUT		FALSE

	FORMATS				${PLUGIN_FORMATS}
)

target_sources(${PLUGIN_TARGET} PRIVATE
	src/Core/Processor.cpp
	src/Core/Editor.cpp
	src/Core/StereoMask.h
	src/Utils/GainUtils.h
	src/Utils/DelayUtils.h
	src/Utils/ValueMappings.h
	src/Presets.h
	src/Utils/UIComponents.h
)

target_compile_definitions(${PLUGIN_TARGET} PRIVATE
	JUCE_WEB_BROWSER=0
	JUCE_USE_CURL=0
	JUCE_VST3_CAN_REPLACE_VST2=0
	JUCE_DISPLAY_SPLASH_SCREEN=0
	JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(${PLUGIN_TARGET} PRIVATE
	juce::juce_dsp
	juce::juce_audio_utils
	juce::juce_gui_extra
	juce::juce_recommended_config_flags
	juce::juce_recommended_lto_flags
	juce::juce_recommended_warning_flags
)