/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"

#include "PluginProcessor.h"

//==============================================================================
SimpleEqAudioProcessorEditor::SimpleEqAudioProcessorEditor(
    SimpleEqAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq",
                               peakFreqSlider),
      peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain",
                               peakGainSlider),
      peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality",
                                  peakQualitySlider),
      lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq",
                                 lowCutFreqSlider),
      highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq",
                                  highCutFreqSlider),
      lowCutSlopeAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlope),
      highCutSlopeAttachment(audioProcessor.apvts, "HighCut Slope",
                             highCutSlope) {
  // Make sure that before the constructor has finished, you've set the
  // editor's size to whatever you need it to be.
  for (auto* comp : getComps()) {
    addAndMakeVisible(comp);
  }
  setSize(600, 400);
}

SimpleEqAudioProcessorEditor::~SimpleEqAudioProcessorEditor() {}

//==============================================================================
void SimpleEqAudioProcessorEditor::paint(juce::Graphics& g) {
  // (Our component is opaque, so we must completely fill the background with a
  // solid colour)
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  g.setColour(juce::Colours::white);
  g.setFont(15.0f);
  g.drawFittedText("Hello World!", getLocalBounds(),
                   juce::Justification::centred, 1);
}

void SimpleEqAudioProcessorEditor::resized() {
  // This is generally where you'll want to lay out the positions of any
  // subcomponents in your editor..
  auto bounds = getLocalBounds();
  auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);

  auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
  auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

  lowCutFreqSlider.setBounds(
      lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
  lowCutSlope.setBounds(lowCutArea);
  highCutFreqSlider.setBounds(
      highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
  highCutSlope.setBounds(highCutArea);

  peakFreqSlider.setBounds(bounds.removeFromTop((bounds.getHeight() * 0.33)));
  peakGainSlider.setBounds(bounds.removeFromTop((bounds.getHeight() * 0.5)));
  peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> SimpleEqAudioProcessorEditor::getComps() {
  return {&peakFreqSlider,   &peakGainSlider,    &peakQualitySlider,
          &lowCutFreqSlider, &highCutFreqSlider, &lowCutSlope,
          &highCutSlope};
}
