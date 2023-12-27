/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

enum FFTOrder
{
  order2048 = 11,
  order4096 = 12,
  order8192 = 13
};

template<typename BlockType>
struct FFTDataGenerator
{
  // produces the FFT data from an audio buffer
  void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData,
                                  const float negativeInfinity)
  {
    const auto fftSize = getFFTSize();

    fftData.assign(fftData.size(), 0);
    auto* readIndex = audioData.getReadPointer(0);
    std::copy(readIndex, readIndex + fftSize, fftData.begin());

    // first apply a windowing function to our data
    window->multiplyWithWindowingTable(fftData.data(), fftSize);

    // then render our FFT data..
    forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

    int numBins = (int)fftSize / 2;

    // normalize the fft values
    for (int i = 0; i < numBins; ++i) {
      fftData[i] /= (float)numBins;
    }

    // conver them to decibels
    for (int i = 0; i < numBins; ++i) {
      fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInfinity);
    }

    fftDataFifo.push(fftData);
  }

  void changeOrder(FFTOrder newOrder)
  {
    // when we change the order, recreate the window, forwardFFT, fifo, and
    // fftData
    // with new size
    order = newOrder;
    auto fftSize = getFFTSize();
    forwardFFT = std::make_unique<juce::dsp::FFT>(order);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
      fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);
    fftData.clear();
    fftData.resize(fftSize * 2, 0);
    fftDataFifo.prepare(fftData.size());
  }
  //===========================================================================
  int getFFTSize() const { return 1 << order; }
  int getNumAvailbleFFTDataBlocks() const
  {
    return fftDataFifo.getNumAvailableForReading();
  }
  //===========================================================================
  bool getFFTData(BlockType& fftData) { return fftDataFifo.pull(fftData); }

private:
  FFTOrder order;
  BlockType fftData;
  std::unique_ptr<juce::dsp::FFT> forwardFFT;

  std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

  Fifo<BlockType> fftDataFifo;
};

template<typename PathType>
struct AnalyzerPathGenerator
{
  /* converts 'renderData[] into a juce::Path*/
  void generatePath(const std::vector<float>& renderData,
                    juce::Rectangle<float> fftBounds,
                    int fftSize,
                    float binWidth,
                    float negativeInfinity)
  {
    auto top = fftBounds.getY();
    auto bottom = fftBounds.getHeight();
    auto width = fftBounds.getWidth();

    int numBins = (int)fftSize / 2;

    PathType p;
    p.preallocateSpace(3 * (int)width);
    auto map = [bottom, top, negativeInfinity](float v) {
      return juce::jmap(v, negativeInfinity, 0.f, float(bottom), top);
    };

    auto y = map(renderData[0]);

    jassert(!std::isnan(y) && !std::isinf(y));

    p.startNewSubPath(0, y);

    const int pathResolution =
      2; // you can draw line-to's every 'pathResolution'  pixels
    for (int binNum = 1; binNum < numBins; binNum += pathResolution) {
      y = map(renderData[binNum]);

      jassert(!std::isnan(y) && !std::isinf(y));

      if (!std::isnan(y) && !std::isinf(y)) {
        auto binFreq = binNum * binWidth;
        auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
        auto binX = std::floor(width * normalizedBinX);
        p.lineTo(binX, y);
      }
    }
    pathFifo.push(p);
  }

  int getNumPathsAvailable() const
  {
    return pathFifo.getNumAvailableForReading();
  }

  bool getPath(PathType& path) { return pathFifo.pull(path); }

private:
  Fifo<PathType> pathFifo;
};

struct LookAndFeel : juce::LookAndFeel_V4
{
  void drawRotarySlider(juce::Graphics&,
                        int x,
                        int y,
                        int width,
                        int height,
                        float sliderPosProportional,
                        float rotaryStartAngle,
                        float rotaryEndAngle,
                        juce::Slider&) override;
};

struct RotarySliderWithLabels : juce::Slider
{
  RotarySliderWithLabels(juce::RangedAudioParameter& rap,
                         const juce::String& unitSuffix)
    : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                   juce::Slider::TextEntryBoxPosition::NoTextBox)
    , param(&rap)
    , suffix(unitSuffix)
  {
    setLookAndFeel(&lnf);
  }
  ~RotarySliderWithLabels() { setLookAndFeel(nullptr); }

  struct LabelPos
  {
    float pos;
    juce::String label;
  };

  juce::Array<LabelPos> labels;

  void paint(juce::Graphics& g) override;
  juce::Rectangle<int> getSliderBounds() const;
  int getTextHeight() const { return 14; }
  juce::String getDisplayString() const;

private:
  LookAndFeel lnf;

  juce::RangedAudioParameter* param;
  juce::String suffix;
};

struct PathProducer
{
  PathProducer(SingleChannelSampleFifo<SimpleEqAudioProcessor::BlockType>& scsf)
    : leftChannelFifo(&scsf)
  {
    leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
    monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
  }
  void process(juce::Rectangle<float> fftBounds, double sampleRate);
  juce::Path getPath() { return leftChannelFFTPath; }

private:
  SingleChannelSampleFifo<SimpleEqAudioProcessor::BlockType>* leftChannelFifo;
  juce::AudioBuffer<float> monoBuffer;

  FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;

  AnalyzerPathGenerator<juce::Path> pathProducer;

  juce::Path leftChannelFFTPath;
};

struct ResponseCurveComponent
  : juce::Component
  , juce::AudioProcessorParameter::Listener
  , juce::Timer
{
  ResponseCurveComponent(SimpleEqAudioProcessor&);
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
                               bool gestureIsStarting) override
  {
  }

  /** The user-defined callback routine that actually gets called periodically.

It's perfectly ok to call startTimer() or stopTimer() from within this
callback to change the subsequent intervals.
*/
  void timerCallback() override;

  void paint(juce::Graphics&) override;

  void resized() override;

private:
  SimpleEqAudioProcessor& audioProcessor;
  juce::Atomic<bool> parametersChanged{ false };
  MonoChain monoChain;

  void updateChain();

  juce::Image background;

  juce::Rectangle<int> getRenderArea();
  juce::Rectangle<int> getAnalysisArea();

  PathProducer leftPathProducer, rightPathProducer;
};

//==============================================================================
/**
 */
class SimpleEqAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
  SimpleEqAudioProcessorEditor(SimpleEqAudioProcessor&);
  ~SimpleEqAudioProcessorEditor() override;

  //==============================================================================
  void paint(juce::Graphics&) override;
  void resized() override;

private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  SimpleEqAudioProcessor& audioProcessor;

  RotarySliderWithLabels peakFreqSlider, peakGainSlider, peakQualitySlider,
    lowCutFreqSlider, highCutFreqSlider, lowCutSlopeSlider, highCutSlopeSlider;

  ResponseCurveComponent responseCurveComponent;

  using APVTS = juce::AudioProcessorValueTreeState;
  using Attachment = APVTS::SliderAttachment;
  Attachment peakFreqSliderAttachment, peakGainSliderAttachment,
    peakQualitySliderAttachment, lowCutFreqSliderAttachment,
    highCutFreqSliderAttachment, lowCutSlopeAttachment, highCutSlopeAttachment;

  std::vector<juce::Component*> getComps();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleEqAudioProcessorEditor)
};
