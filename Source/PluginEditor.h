/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

struct LookAndFeel : juce::LookAndFeel_V4 {
  void drawRotarySlider(juce::Graphics &, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &) override{};
};

struct RotarySliderWithLabels : juce::Slider {
  RotarySliderWithLabels(juce::RangedAudioParameter &rap,
                         const juce::String &unitSuffix)
      : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                     juce::Slider::TextEntryBoxPosition::NoTextBox),
        param(&rap),
        suffix(unitSuffix) {
    setLookAndFeel(&lnf);
  }
  ~RotarySliderWithLabels() { setLookAndFeel(nullptr); }

  void paint(juce::Graphics &g) override{};
  juce::Rectangle<int> getSliderBounds() const;
  int getTextHeight() const { return 14; }
  juce::String getDisplayString() const;

 private:
  LookAndFeel lnf;

  juce::RangedAudioParameter *param;
  juce::String suffix;
};

struct ResponseCurveComponent : juce::Component,
                                juce::AudioProcessorParameter::Listener,
                                juce::Timer {
  ResponseCurveComponent(SimpleEqAudioProcessor &);
  ~ResponseCurveComponent();

  /** Receives a callback when a parameter has been changed.

          IMPORTANT NOTE: This will be called synchronously when a parameter
   changes, and many audio processors will change their parameter during their
   audio callback. This means that not only has your handler code got to be
   completely thread-safe, but it's also got to be VERY fast, and avoid
   blocking. If you need to handle this event on your message thread, use this
   callback to trigger an AsyncUpdater or ChangeBroadcaster which you can
   respond to on the message thread.
      */
  void parameterValueChanged(int parameterIndex, float newValue) override;

  /** Indicates that a parameter change gesture has started.

      E.g. if the user is dragging a slider, this would be called with
     gestureIsStarting being true when they first press the mouse button, and it
     will be called again with gestureIsStarting being false when they release
     it.

      IMPORTANT NOTE: This will be called synchronously, and many audio
     processors will call it during their audio callback. This means that not
     only has your handler code got to be completely thread-safe, but it's also
     got to be VERY fast, and avoid blocking. If you need to handle this event
     on your message thread, use this callback to trigger an AsyncUpdater or
     ChangeBroadcaster which you can respond to later on the message thread.
  */
  void parameterGestureChanged(int parameterIndex,
                               bool gestureIsStarting) override {}

  /** The user-defined callback routine that actually gets called periodically.

It's perfectly ok to call startTimer() or stopTimer() from within this
callback to change the subsequent intervals.
*/
  void timerCallback() override;

  void paint(juce::Graphics &) override;

 private:
  SimpleEqAudioProcessor &audioProcessor;
  juce::Atomic<bool> parametersChanged{false};
  MonoChain monoChain;
};

//==============================================================================
/**
 */
class SimpleEqAudioProcessorEditor : public juce::AudioProcessorEditor {
 public:
  SimpleEqAudioProcessorEditor(SimpleEqAudioProcessor &);
  ~SimpleEqAudioProcessorEditor() override;

  //==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;

 private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  SimpleEqAudioProcessor &audioProcessor;

  RotarySliderWithLabels peakFreqSlider, peakGainSlider, peakQualitySlider,
      lowCutFreqSlider, highCutFreqSlider, lowCutSlopeSlider,
      highCutSlopeSlider;

  ResponseCurveComponent responseCurveComponent;

  using APVTS = juce::AudioProcessorValueTreeState;
  using Attachment = APVTS::SliderAttachment;
  Attachment peakFreqSliderAttachment, peakGainSliderAttachment,
      peakQualitySliderAttachment, lowCutFreqSliderAttachment,
      highCutFreqSliderAttachment, lowCutSlopeAttachment,
      highCutSlopeAttachment;

  std::vector<juce::Component *> getComps();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleEqAudioProcessorEditor)
};
