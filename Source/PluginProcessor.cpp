/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"

#include "PluginEditor.h"

//==============================================================================
SimpleEqAudioProcessor::SimpleEqAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
#endif
{
}

SimpleEqAudioProcessor::~SimpleEqAudioProcessor() {}

//==============================================================================
const juce::String SimpleEqAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool SimpleEqAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool SimpleEqAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool SimpleEqAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double SimpleEqAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int SimpleEqAudioProcessor::getNumPrograms() {
  return 1;  // NB: some hosts don't cope very well if you tell them there are 0
             // programs, so this should be at least 1, even if you're not
             // really implementing programs.
}

int SimpleEqAudioProcessor::getCurrentProgram() { return 0; }

void SimpleEqAudioProcessor::setCurrentProgram(int index) {}

const juce::String SimpleEqAudioProcessor::getProgramName(int index) {
  return {};
}

void SimpleEqAudioProcessor::changeProgramName(int index,
                                               const juce::String& newName) {}

//==============================================================================
void SimpleEqAudioProcessor::prepareToPlay(double sampleRate,
                                           int samplesPerBlock) {
  // Use this method as the place to do any pre-playback
  // initialisation that you need..
  juce::dsp::ProcessSpec spec;
  spec.maximumBlockSize = samplesPerBlock;
  spec.numChannels = 1;
  spec.sampleRate = sampleRate;

  leftChain.prepare(spec);
  rightChain.prepare(spec);

  updateFilters();
}

void SimpleEqAudioProcessor::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEqAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}
#endif

void SimpleEqAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  // In case we have more outputs than inputs, this code clears any output
  // channels that didn't contain input data, (because these aren't
  // guaranteed to be empty - they may contain garbage).
  // This is here to avoid people getting screaming feedback
  // when they first compile a plugin, but obviously you don't need to keep
  // this code if your algorithm always overwrites all the output channels.
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  updateFilters();

  juce::dsp::AudioBlock<float> block(buffer);

  auto leftBlock = block.getSingleChannelBlock(0);
  auto rightBlock = block.getSingleChannelBlock(1);

  juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
  juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

  leftChain.process(leftContext);
  rightChain.process(rightContext);

  //    // This is the place where you'd normally do the guts of your plugin's
  //    // audio processing...
  //    // Make sure to reset the state if your inner loop is processing
  //    // the samples and the outer loop is handling the channels.
  //    // Alternatively, you can process the samples with the channels
  //    // interleaved by keeping the same state.
  //    for (int channel = 0; channel < totalNumInputChannels; ++channel)
  //    {
  ////        auto* channelData = buffer.getWritePointer (channel);
  //
  //        // ..do something to the data...
  //    }
}

//==============================================================================
bool SimpleEqAudioProcessor::hasEditor() const {
  return true;  // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEqAudioProcessor::createEditor() {
  return new SimpleEqAudioProcessorEditor(*this);
  // return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleEqAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
  juce::MemoryOutputStream mos(destData, true);
  apvts.state.writeToStream(mos);
}

void SimpleEqAudioProcessor::setStateInformation(const void* data,
                                                 int sizeInBytes) {
  // You should use this method to restore your parameters from this memory
  // block, whose contents will have been created by the getStateInformation()
  // call.
  auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
  if (tree.isValid()) {
    apvts.replaceState(tree);
    updateFilters();
  }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
  ChainSettings settings;

  settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
  settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
  settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
  settings.peakGainInDecibles = apvts.getRawParameterValue("Peak Gain")->load();
  settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
  settings.lowCutSlope =
      static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
  settings.highCutSlope =
      static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
  return settings;
}

Coefficients makePeakFilter(const ChainSettings& chainSettings,
                            double sampleRate) {
  return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
      sampleRate, chainSettings.peakFreq, chainSettings.peakQuality,
      juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibles));
}

void SimpleEqAudioProcessor::updatePeakFilter(
    const ChainSettings& chainSettings) {
  auto peakCoefficients = makePeakFilter(chainSettings, getSampleRate());

  updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients,
                     peakCoefficients);
  updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients,
                     peakCoefficients);
}

void updateCoefficients(Coefficients& old, const Coefficients& replacements) {
  *old = *replacements;
}

void SimpleEqAudioProcessor::updateLowCutFilters(
    const ChainSettings& chainSettings) {
  auto cutCoefficients = makeLowCutFilter(chainSettings, getSampleRate());

  auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
  auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();

  updateCutFilter(leftLowCut, cutCoefficients, chainSettings.lowCutSlope);
  updateCutFilter(rightLowCut, cutCoefficients, chainSettings.lowCutSlope);
}

void SimpleEqAudioProcessor::updateHighCutFilters(
    const ChainSettings& chainSettings) {
  auto highCutCoefficients = makeHighCutFilter(chainSettings, getSampleRate());

  auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
  auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();

  updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
  updateCutFilter(rightHighCut, highCutCoefficients,
                  chainSettings.highCutSlope);
}

void SimpleEqAudioProcessor::updateFilters() {
  auto chainSettings = getChainSettings(apvts);

  updateLowCutFilters(chainSettings);
  updatePeakFilter(chainSettings);
  updateHighCutFilters(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout
SimpleEqAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID("LowCut Freq", 1), "LowCut Freq",
      juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20.f));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID("HighCut Freq", 1), "HighCut Freq",
      juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20000.f));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID("Peak Freq", 1), "Peak Freq",
      juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 750.f));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID("Peak Gain", 1), "Peak Gain",
      juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f), 0.f));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID("Peak Quality", 1), "Peak Quality",
      juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));

  juce::StringArray stringArray;
  for (int i = 0; i < 4; ++i) {
    juce::String str;
    str << (12 + i * 12);
    str << " db/Oct";
    stringArray.add(str);
  }

  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("LowCut Slope", 1), "LowCut Slope", stringArray, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("HighCut Slope", 1), "HighCut Slope", stringArray, 0));
  return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new SimpleEqAudioProcessor();
}
