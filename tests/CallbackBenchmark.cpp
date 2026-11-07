#include "PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <sys/resource.h>
#endif
static juce::MemoryBlock donor()
{
    juce::AudioBuffer<float> b(1, 24000);
    for (int i = 0; i < 24000; ++i)
        b.setSample(0, i,
                    .3f * std::sin(i * .12f) * (i < 12000 ? 1 : .2f) +
                        .15f * std::sin(i * (i < 12000 ? .21f : .33f)));
    juce::MemoryBlock bytes;
    juce::WavAudioFormat format;
    {
        std::unique_ptr<juce::AudioFormatWriter> w(
            format.createWriterFor(new juce::MemoryOutputStream(bytes, false), 48000, 1, 16, {}, 0));
        if (!w || !w->writeFromAudioSampleBuffer(b, 0, 24000))
            throw std::runtime_error("Donor failed");
    }
    return bytes;
}
static std::uint64_t memory()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS p{};
    GetProcessMemoryInfo(GetCurrentProcess(), &p, sizeof(p));
    return p.WorkingSetSize;
#else
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return std::uint64_t(r.ru_maxrss) * 1024;
#endif
}
static std::pair<std::uint64_t, double> processTree()
{
#ifdef _WIN32
    std::vector<DWORD> owned{GetCurrentProcessId()};
    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    std::vector<std::pair<DWORD, DWORD>> all;
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        if (Process32First(snapshot, &entry))
            do
            {
                all.push_back({entry.th32ProcessID, entry.th32ParentProcessID});
            } while (Process32Next(snapshot, &entry));
        CloseHandle(snapshot);
    }
    for (std::size_t i = 0; i < owned.size(); ++i)
        for (auto child : all)
            if (child.second == owned[i] && std::find(owned.begin(), owned.end(), child.first) == owned.end())
                owned.push_back(child.first);
    std::uint64_t bytes = 0;
    double cpu = 0;
    for (auto pid : owned)
    {
        auto handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!handle)
            continue;
        PROCESS_MEMORY_COUNTERS memory{};
        if (GetProcessMemoryInfo(handle, &memory, sizeof(memory)))
            bytes += memory.WorkingSetSize;
        FILETIME creation{}, exit{}, kernel{}, user{};
        if (GetProcessTimes(handle, &creation, &exit, &kernel, &user))
        {
            ULARGE_INTEGER k{}, u{};
            k.LowPart = kernel.dwLowDateTime;
            k.HighPart = kernel.dwHighDateTime;
            u.LowPart = user.dwLowDateTime;
            u.HighPart = user.dwHighDateTime;
            cpu += double(k.QuadPart + u.QuadPart) * 1e-7;
        }
        CloseHandle(handle);
    }
    return {bytes, cpu};
#else
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return {memory(), double(r.ru_utime.tv_sec + r.ru_stime.tv_sec) +
                          double(r.ru_utime.tv_usec + r.ru_stime.tv_usec) * 1e-6};
#endif
}
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const std::filesystem::path dir = argc > 1 ? argv[1] : "CallbackResults";
        std::filesystem::create_directories(dir);
        const juce::String label = argc > 2 ? argv[2] : "current";
        const bool ui = argc > 3 && std::string(argv[3]) == "ui";
        const int model=argc>4?std::stoi(argv[4]):4;
        if(model!=3&&model!=4)throw std::runtime_error("Benchmark model must be 3 or 4");
        std::ofstream report(dir / (label.toStdString() + "-callback.json"));
        report << "{\"label\":\"" << label << "\",\"cpu\":\"" << juce::SystemStats::getCpuVendor() << " "
               << juce::SystemStats::getCpuModel() << "\",\"logicalCpus\":" << juce::SystemStats::getNumCpus()
               << ",\"os\":\"" << juce::SystemStats::getOperatingSystemName()
               << "\",\"processingModel\":" << model << ",\"editorOpen\":" << (ui ? "true" : "false") << ",\"cases\":[";
        bool first = true;
        double worstPeak = 0;
        WubrotProcessor p;
        juce::MemoryBlock state;p.getStateInformation(state);
        auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
        xml->setAttribute("xyMapping",model);juce::AudioProcessor::copyXmlToBinary(*xml,state);
        p.setStateInformation(state.getData(),int(state.getSize()));
        if(int(p.uiState()["xyMapping"])!=model)throw std::runtime_error("Requested benchmark model was not applied");
        p.setParameterValue("x", .55f);
        p.setParameterValue("y", -.45f);
        p.setParameterValue("drive", 12);
        p.setParameterValue("mutate", 100);
        p.loadSampleBytes(donor(), "synthetic-benchmark.wav");
        for (int i = 0; i < 10000 && p.isLoading(); ++i)
            juce::Thread::sleep(2);
        if (p.isLoading())
            throw std::runtime_error("Analysis timeout");
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        juce::Component window;
        if (ui)
        {
            editor.reset(p.createEditorIfNeeded());
            window.setBounds(10, 10, 804, 697);
            window.addAndMakeVisible(*editor);
            window.addToDesktop(juce::ComponentPeer::windowHasTitleBar);
            window.setVisible(true);
        }
        std::atomic<bool> complete{false};
        std::exception_ptr failure;
        std::pair<std::uint64_t, double> beforeCpu{};
        std::thread audio([&] {
            try
            {
                if (ui)
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                beforeCpu = processTree();
                for (int rate : {44100, 48000, 96000})
                    for (int block : {64, 128, 512})
                    {
                        p.prepareToPlay(rate, block);
                        juce::AudioBuffer<float> b(2, block);
                        juce::MidiBuffer events;
                        std::vector<double> durations(160);
                        int frame = 0;
                        auto next = std::chrono::steady_clock::now();
                        for (int n = -20; n < 160; ++n)
                        {
                            for (int i = 0; i < block; ++i, ++frame)
                            {
                                double t = double(frame) / rate;
                                float v = float((.3 * std::sin(t * 345.575) + .08 * std::sin(t * 2073.45)) *
                                                (std::fmod(t, .3) < .15 ? 1 : .25));
                                b.setSample(0, i, v);
                                b.setSample(1, i, v);
                            }
                            const auto start = std::chrono::steady_clock::now();
                            p.processBlock(b, events);
                            const auto end = std::chrono::steady_clock::now();
                            if (n >= 0)
                                durations[n] = std::chrono::duration<double, std::micro>(end - start).count();
                            for (int c = 0; c < 2; ++c)
                                for (int i = 0; i < block; ++i)
                                {
                                    auto v = b.getSample(c, i);
                                    if (!std::isfinite(v))
                                        throw std::runtime_error("Nonfinite benchmark audio");
                                    worstPeak = std::max(worstPeak, double(std::abs(v)));
                                }
                            next += std::chrono::nanoseconds(std::int64_t(1e9 * block / rate));
                            std::this_thread::sleep_until(next);
                        }
                        double average = 0;
                        for (double d : durations)
                            average += d / durations.size();
                        std::sort(durations.begin(), durations.end());
                        if (!first)
                            report << ',';
                        first = false;
                        report << "{\"sampleRate\":" << rate << ",\"buffer\":" << block
                               << ",\"channels\":2,\"callbacks\":160,\"meanUs\":" << average
                               << ",\"p95Us\":" << durations[151] << ",\"p99Us\":" << durations[158]
                               << ",\"maxUs\":" << durations.back()
                               << ",\"deadlineUs\":" << 1e6 * block / rate
                               << ",\"residentBytes\":" << memory() << '}';
                    }
                // Identical synthetic input and quadrant positions, stored without level matching.
                p.setParameterValue("drive", 0);
                for (int quadrant = 0; quadrant < 4; ++quadrant)
                {
                    p.setParameterValue("x", quadrant & 1 ? .5f : -.5f);
                    p.setParameterValue("y", quadrant & 2 ? .5f : -.5f);
                    p.prepareToPlay(48000, 128);
                    juce::AudioBuffer<float> out(2, 48000);
                    for (int off = 0; off < 48000; off += 128)
                    {
                        const int count = std::min(128, 48000 - off);
                        juce::AudioBuffer<float> b(2, count);
                        for (int i = 0; i < count; ++i)
                        {
                            float v = .3f * std::sin((off + i) * .007199483f) +
                                      .1f * std::sin((off + i) * .0431969f);
                            b.setSample(0, i, v);
                            b.setSample(1, i, v);
                        }
                        juce::MidiBuffer midi;
                        p.processBlock(b, midi);
                        for (int c = 0; c < 2; ++c)
                            out.copyFrom(c, off, b, c, 0, count);
                    }
                    juce::WavAudioFormat format;
                    auto file = juce::File(juce::String(
                        (dir / (label.toStdString() + "-quadrant-" + std::to_string(quadrant) + ".wav"))
                            .string()));
                    std::unique_ptr<juce::AudioFormatWriter> w(
                        format.createWriterFor(file.createOutputStream().release(), 48000, 2, 24, {}, 0));
                    if (!w || !w->writeFromAudioSampleBuffer(out, 0, 48000))
                        throw std::runtime_error("Baseline render failed");
                }
            }
            catch (...)
            {
                failure = std::current_exception();
            }
            complete.store(true);
            if (ui)
                juce::MessageManager::callAsync(
                    [] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
        });
        if (ui)
            juce::MessageManager::getInstance()->runDispatchLoop();
        audio.join();
        const auto tree = processTree();
        if (editor)
        {
            window.removeChildComponent(editor.get());
            window.removeFromDesktop();
            editor.reset();
        }
        if (failure)
            std::rethrow_exception(failure);
        report << "],\"processTreeResidentBytes\":" << tree.first
               << ",\"processTreeCpuSeconds\":" << tree.second - beforeCpu.second
               << ",\"ramMiB\":" << juce::SystemStats::getMemorySizeInMegabytes()
               << ",\"latencySamples\":" << p.getLatencySamples() << ",\"uiWarmupSeconds\":" << (ui ? 5 : 0)
               << ",\"observedPeak\":" << worstPeak
               << ",\"memoryNote\":\"Linux reports process peak RSS; Windows reports current working set. "
                  "Timings measure processBlock only under paced callbacks, not offline throughput or "
                  "scheduling lateness.\"}";
        report.close();
        std::cout << "Paced actual processBlock measurements: " << label << " UI=" << ui
                  << " peak=" << worstPeak << '\n';
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
