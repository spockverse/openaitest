#pragma once
#include <juce_core/juce_core.h>
#include <optional>
#include <atomic>

namespace WubrotUpdates
{
struct Release
{
    juce::String version, summary, page, download, sha256;
    juce::int64 bytes=0;
    juce::var notice(const juce::String& installed) const;
};
// Only a newer, published stable release with a Windows x64 ZIP is offered.
std::optional<Release> parseRelease(const juce::String& json, const juce::String& installed);
class Transport
{
public:
    virtual ~Transport() = default;
    virtual juce::String fetch() = 0;
    virtual void cancel() = 0;
};
class DownloadTask
{
public:
    virtual ~DownloadTask()=default;
    virtual void start()=0;
    virtual void cancel()=0;
    virtual juce::var snapshot() const=0;
};
using DownloadFactory=std::function<std::unique_ptr<DownloadTask>(const Release&)>;
// Editor-only process-session service. No processor or audio-thread access.
// Editors may close while the bounded request finishes; no callback captures them.
class Session final : private juce::Thread
{
public:
    Session(juce::String installed, std::unique_ptr<Transport>, bool enabled = true,DownloadFactory = {});
    ~Session() override;
    static Session& shared(const juce::String& installed);
    void checkOnce();
    bool finished() const noexcept { return done.load(std::memory_order_acquire); }
    juce::var claim(const void* editor);
    void release(const void* editor);
    void shown(const void* editor, const juce::String& version);
    void later(const void* editor, const juce::String& version);
    juce::var reopen(const void* editor, const juce::String& version);
    juce::Result download(const void* editor,const juce::String& version);
    void cancel(const void* editor,const juce::String& version);
    juce::var progress(const void* editor);
    juce::String link(const void* editor, const juce::String& version, bool notes);
private:
    void run() override;
    bool owns(const void*, const juce::String&) const;
    const juce::String installed;
    std::unique_ptr<Transport> transport;
    const bool enabled;
    std::atomic<bool> started {false}, done {false};
    juce::CriticalSection lock;
    std::optional<Release> result;
    const void* owner = nullptr;
    bool announced = false, downloadDismissed=false;
    DownloadFactory factory;
    std::unique_ptr<DownloadTask> task;
};
}
