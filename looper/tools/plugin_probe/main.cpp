#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <iostream>

static void line(const char* key, const juce::String& value) { std::cout << key << "=" << value.toStdString() << std::endl; }

int main(int argc, char** argv)
{
    if (argc != 4) { std::cerr << "usage: plugin_probe scan|run plugin.vst3 output.xml|output.wav\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::String mode(argv[1]), path(argv[2]);
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile(found, path);
    line("scan_count", juce::String(found.size()));
    if (found.isEmpty()) return 3;
    juce::KnownPluginList known;
    for (auto* desc : found) {
        known.addType(*desc);
        line("name", desc->name);
        line("identifier", desc->createIdentifierString());
        line("version", desc->version);
    }
    if (mode == "scan") {
        if (!known.createXml()->writeTo(juce::File(argv[3]))) return 4;
        return 0;
    }
    if (mode != "run") return 2;

    juce::String error;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256, total = 48000;
    juce::AudioPluginFormatManager manager;
    manager.addFormat(new juce::VST3PluginFormat());
    auto create = [&]() { return manager.createPluginInstance(*found[0], sampleRate, blockSize, error); };
    auto plugin = create();
    if (!plugin) { line("load_error", error); return 5; }
    line("load", "ok");
    const bool instrument = plugin->isMidiEffect() || found[0]->isInstrument;
    line("instrument", instrument ? "true" : "false");
    line("parameter_count", juce::String(plugin->getParameters().size()));
    plugin->setPlayHead(nullptr);
    plugin->prepareToPlay(sampleRate, blockSize);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(500);
    line("input_channels", juce::String(plugin->getTotalNumInputChannels()));
    line("output_channels", juce::String(plugin->getTotalNumOutputChannels()));
    line("suspended", plugin->isSuspended() ? "true" : "false");
    juce::AudioBuffer<float> captured(2, total);
    captured.clear();
    auto render = [&](juce::AudioProcessor& processor, juce::AudioBuffer<float>& output) {
        const int channels = std::max(2, std::max(processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels()));
        juce::AudioBuffer<float> block(channels, blockSize);
        double sum = 0, max = 0, diff = 0;
        for (int pos = 0; pos < total; pos += blockSize) {
            const int n = std::min(blockSize, total - pos);
            block.clear();
            juce::MidiBuffer midi;
            if (instrument) {
                if (pos == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
                if (pos <= 24000 && pos + n > 24000) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 24000 - pos);
            } else {
                for (int i = 0; i < n; ++i) {
                    const float input = 0.12f * std::sin(2.0 * juce::MathConstants<double>::pi * 220.0 * (pos + i) / sampleRate);
                    block.setSample(0, i, input);
                    block.setSample(1, i, input);
                }
            }
            processor.processBlock(block, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) {
                const double sample = block.getSample(ch, i);
                if (!std::isfinite(sample)) return juce::String("nonfinite");
                output.setSample(ch, pos + i, (float)sample);
                sum += sample * sample;
                max = std::max(max, std::abs(sample));
                if (!instrument) {
                    const double input = 0.12 * std::sin(2.0 * juce::MathConstants<double>::pi * 220.0 * (pos + i) / sampleRate);
                    diff += (sample - input) * (sample - input);
                }
            }
        }
        return juce::String("rms:") + juce::String(std::sqrt(sum / (2 * total)), 8) +
               ",peak:" + juce::String(max, 8) + ",diff_rms:" + juce::String(std::sqrt(diff / (2 * total)), 8);
    };
    const auto baseline = render(*plugin, captured);
    line("baseline", baseline);
    if (baseline == "nonfinite") return 6;
    juce::MemoryBlock stateBefore, stateAfter;
    plugin->getStateInformation(stateBefore);
    line("state_before_bytes", juce::String((int)stateBefore.getSize()));
    int mutatedIndex = -1;
    float mutatedValue = 0.0f;
    for (auto* param : plugin->getParameters()) {
        if (param->isAutomatable() && param->getNumSteps() > 128) {
            const float old = param->getValue();
            const float changed = old < 0.5f ? std::min(1.0f, old + 0.15f) : std::max(0.0f, old - 0.15f);
            param->setValueNotifyingHost(changed);
            mutatedIndex = plugin->getParameters().indexOf(param);
            mutatedValue = param->getValue();
            line("mutated_parameter", param->getName(80) + ":" + juce::String(old, 4) + "->" + juce::String(param->getValue(), 4));
            break;
        }
    }
    plugin->getStateInformation(stateAfter);
    line("state_after_bytes", juce::String((int)stateAfter.getSize()));
    plugin->releaseResources();
    plugin.reset();
    auto restored = create();
    if (!restored) { line("reload_error", error); return 7; }
    restored->setStateInformation(stateAfter.getData(), (int)stateAfter.getSize());
    if (mutatedIndex >= 0 && mutatedIndex < restored->getParameters().size()) {
        const float actual = restored->getParameters()[mutatedIndex]->getValue();
        line("restored_parameter", juce::String(actual, 6));
        line("parameter_match", std::abs(actual - mutatedValue) < 0.001f ? "true" : "false");
    }
    restored->prepareToPlay(sampleRate, blockSize);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(500);
    juce::AudioBuffer<float> restoredAudio(2, total);
    restoredAudio.clear();
    const auto restoredMetrics = render(*restored, restoredAudio);
    line("restored", restoredMetrics);
    if (restoredMetrics == "nonfinite") return 8;
    juce::MemoryBlock stateReloaded;
    restored->getStateInformation(stateReloaded);
    line("state_reloaded_bytes", juce::String((int)stateReloaded.getSize()));
    line("state_exact_match", stateAfter == stateReloaded ? "true" : "false");
    restored->releaseResources();
    auto file = juce::File(argv[3]);
    file.deleteFile();
    auto stream = file.createOutputStream();
    if (!stream) return 9;
    juce::WavAudioFormat wav;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.release(), sampleRate, 2, 24, {}, 0));
    if (!writer || !writer->writeFromAudioSampleBuffer(restoredAudio, 0, total)) return 10;
    line("wav", file.getFullPathName());
    return 0;
}
