#include "UpdateChecker.h"
#include "UpdatePackage.h"
#include <cmath>
#include <stdexcept>
#if JUCE_WINDOWS
#include <windows.h>
#include <string>
#endif
#include <array>
#include <limits>

namespace WubrotUpdates
{
namespace
{
constexpr const char* repository = "https://github.com/spockverse/openaitest";
struct Version { std::array<int,3> numbers {}; bool prerelease = false; };
bool identifiers(const juce::String& text,bool prerelease)
{
    if(text.isEmpty()||!text.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-"))return false;
    const auto parts=juce::StringArray::fromTokens(text,".","");if(parts.joinIntoString(".")!=text)return false;
    for(const auto& part:parts)if(part.isEmpty()||(prerelease&&part.containsOnly("0123456789")&&part.length()>1&&part.startsWithChar('0')))return false;
    return true;
}
std::optional<Version> version(juce::String text)
{
    if (text.startsWithChar('v')) text = text.substring(1);
    if (text.isEmpty() || text.length() > 80 || text.containsAnyOf(" /\\\r\n\t")) return {};
    const auto plus = text.indexOfChar('+');
    if (plus >= 0) {
        const auto build = text.substring(plus+1);
        if (!identifiers(build,false)) return {};
        text = text.substring(0,plus);
    }
    Version value;
    const auto dash = text.indexOfChar('-');
    if (dash >= 0) {
        const auto pre = text.substring(dash+1);
        if (!identifiers(pre,true)) return {};
        value.prerelease = true; text = text.substring(0,dash);
    }
    const auto parts = juce::StringArray::fromTokens(text,".","");
    if (parts.size() != 3 || parts.joinIntoString(".")!=text) return {};
    for (int i=0;i<3;++i) {
        const auto& part=parts[i];
        if (part.isEmpty() || !part.containsOnly("0123456789") || (part.length()>1 && part.startsWithChar('0')) || part.length()>10) return {};
        const auto number=part.getLargeIntValue();
        if (number>65535) return {}; // Windows VERSIONINFO stores 16-bit components.
        value.numbers[static_cast<size_t>(i)]=static_cast<int>(number);
    }
    return value;
}
juce::String summary(const juce::String& body)
{
    juce::String out;
    for (auto line : juce::StringArray::fromLines(body.substring(0,8192))) {
        line=line.trim();
        if (line.isEmpty()) { if(out.isNotEmpty()) break; else continue; }
        if (line.startsWithChar('#') || line.startsWith("![")) continue;
        if(out.isNotEmpty()) out+=" ";
        out+=line.trimCharactersAtStart("-* ").replace("**","").replace("`","");
        if(out.length()>200) break;
    }
    if(out.isEmpty()) out="A new version of WUBROT is ready. View the release notes for the latest improvements.";
    return out.length()>200 ? out.substring(0,197)+"..." : out;
}
class GitHubTransport final : public Transport
{
public:
    juce::String fetch() override
    {
        auto next=std::make_unique<juce::WebInputStream>(juce::URL("https://api.github.com/repos/spockverse/openaitest/releases/latest"),false);
        next->withConnectionTimeout(3500).withNumRedirectsToFollow(0)
            .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: WUBROT-Update-Checker\r\nX-GitHub-Api-Version: 2022-11-28\r\n");
        juce::WebInputStream* input=nullptr;{const juce::ScopedLock guard(lock);if(cancelled.load())return {};stream=std::move(next);input=stream.get();}
        status=0;const auto deadline=juce::Time::getMillisecondCounterHiRes()+8000.;
        const bool connected=input->connect(nullptr);status=input->getStatusCode();
        if(!connected||status!=200)return {};
        constexpr int limit=256*1024;
        if(input->getTotalLength()>limit)return {};
        juce::MemoryOutputStream data;std::array<char,4096> bytes {};
        while(!input->isExhausted()&&!cancelled.load()&&juce::Time::getMillisecondCounterHiRes()<deadline){
            const auto n=input->read(bytes.data(),static_cast<int>(bytes.size()));if(n<=0)break;
            if(data.getDataSize()+static_cast<size_t>(n)>limit)return {};
            data.write(bytes.data(),static_cast<size_t>(n));
        }
        if(cancelled.load()||input->isError()||!input->isExhausted())return {};
        return data.toUTF8();
    }
    void cancel() override {cancelled.store(true);const juce::ScopedLock guard(lock);if(stream)stream->cancel();}
    int statusCode() const override {return status;}
private:
    juce::CriticalSection lock;std::unique_ptr<juce::WebInputStream> stream;
    std::atomic<bool> cancelled {false};int status=0;
};
}
juce::var Release::notice(const juce::String& installed) const
{
    auto data=std::make_unique<juce::DynamicObject>();
    data->setProperty("phase","available");data->setProperty("total",bytes);data->setProperty("version",version);data->setProperty("installed",installed);data->setProperty("summary",summary);
    return juce::var(data.release()); // URLs stay native and are never accepted from JS.
}
std::optional<Release> parseRelease(const juce::String& json, const juce::String& installed)
{
    if(json.getNumBytesAsUTF8()>256*1024) return {};
    const auto data=juce::JSON::parse(json);
    if(!data.isObject() || !data["draft"].isBool() || !data["prerelease"].isBool() || bool(data["draft"]) || bool(data["prerelease"]) || !data["tag_name"].isString()) return {};
    const auto tag=data["tag_name"].toString();const auto latest=version(tag),current=version(installed);
    if(!latest || !current || latest->prerelease || latest->numbers<current->numbers || (latest->numbers==current->numbers && !current->prerelease)) return {};
    const auto* assets=data["assets"].getArray();
    if(!assets || assets->size()>200) return {};
    for(const auto& asset:*assets) {
        const auto name=asset["name"].toString(),lower=name.toLowerCase();
        if(name.length()>160 || name.isEmpty() || !name.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") || !lower.contains("wubrot") || !lower.contains("windows") || !lower.contains("x64") || !lower.endsWith(".zip") || asset["state"].toString()!="uploaded" || double(asset["size"])<=0) continue;
        if(!asset["size"].isInt()&&!asset["size"].isInt64()&&!asset["size"].isDouble())continue;
        const auto digest=asset["digest"].toString();const double length=double(asset["size"]);
        if(!std::isfinite(length)||length<=0||length>128.*1024.*1024.||length!=std::floor(length)||!digest.startsWith("sha256:")||digest.length()!=71||!digest.substring(7).containsOnly("0123456789abcdefABCDEF"))continue;
        const auto base=juce::String(repository)+"/releases/";
        const auto encoded=juce::URL::addEscapeChars(tag,true);
        return Release{tag.startsWithChar('v')?tag.substring(1):tag,summary(data["body"].toString()),base+"tag/"+encoded,base+"download/"+encoded+"/"+juce::URL::addEscapeChars(name,true),digest.substring(7).toLowerCase(),static_cast<juce::int64>(length)};
    }
    return {};
}
namespace
{
class NativeDownload final : public DownloadTask, private juce::Thread
{
public:
    explicit NativeDownload(Release r):Thread("WUBROT update download"),release(std::move(r)){}
    ~NativeDownload() override {signalThreadShouldExit();cancelStream();stopThread(-1);
       #if JUCE_WINDOWS
        if(helperProcess)CloseHandle(helperProcess);
       #endif
    }
    void start() override {if(!startThread())set("failed","Could not start the update download.");}
    void cancel() override
    {
        cancelRequested.store(true);signalThreadShouldExit();
        if(armed.load()){cancelPrepared();return;}
        cancelStream();
    }
    juce::var snapshot() const override
    {
        const juce::ScopedLock guard(stateLock);auto data=std::make_unique<juce::DynamicObject>();
        auto visiblePhase=phase;if(cancelRequested.load())visiblePhase=armed.load()?phase:(isThreadRunning()?"cancelling":"cancelled");
        if(isThreadRunning()&&(phase=="failed"||phase=="cancelled"))visiblePhase="cancelling";
        data->setProperty("phase",visiblePhase);data->setProperty("message",message);data->setProperty("version",release.version);data->setProperty("received",received.load());data->setProperty("total",release.bytes);return juce::var(data.release());
    }
private:
    void set(const juce::String& p,const juce::String& text={}){const juce::ScopedLock guard(stateLock);phase=p;message=text;}
    void cancelPrepared()
    {
        auto flag=work.getChildFile("cancel.flag");juce::FileOutputStream out(flag);bool written=out.openedOk()&&out.setPosition(0)&&out.writeByte('1');out.flush();written=written&&out.getStatus().wasOk();
       #if JUCE_WINDOWS
        if(!written&&helperProcess)written=TerminateProcess(helperProcess,0)!=0;
       #endif
        set(written?"cancelled":"ready",written?juce::String():"Could not cancel the queued installation. Keep the DAW open and retry Cancel.");
    }
    void cancelStream(){const juce::ScopedLock guard(streamLock);if(stream)stream->cancel();}
    void require(bool condition,const juce::String& text){if(!condition)throw std::runtime_error(text.toStdString());}
    void cancelled(){require(!threadShouldExit(),"Download cancelled.");}
    void transfer(const juce::File& destination)
    {
        auto url=release.download;const auto deadline=juce::Time::getMillisecondCounterHiRes()+30.*60.*1000.;
        juce::WebInputStream* input=nullptr;
        for(int redirect=0;redirect<=5;++redirect){
            cancelled();const juce::URL parsed(url);const auto domain=parsed.getDomain().toLowerCase();
            require((domain=="github.com"||domain=="release-assets.githubusercontent.com"||domain=="objects.githubusercontent.com")&&url.startsWith("https://"+domain+"/"),"The release download redirected to an unsupported location.");
            auto next=std::make_unique<juce::WebInputStream>(parsed,false);next->withConnectionTimeout(5000).withNumRedirectsToFollow(0).withExtraHeaders("User-Agent: WUBROT-Update-Checker\r\nAccept: application/octet-stream\r\n");
            {const juce::ScopedLock guard(streamLock);stream=std::move(next);input=stream.get();}
            cancelled();require(input->connect(nullptr),"Could not connect to GitHub. Check your connection and retry.");
            const int status=input->getStatusCode();if(status==301||status==302||status==303||status==307||status==308){const auto headers=input->getResponseHeaders();url.clear();for(int i=0;i<headers.size();++i)if(headers.getAllKeys()[i].equalsIgnoreCase("location"))url=headers.getAllValues()[i].trim();require(url.length()<16384,"Invalid download redirect.");continue;}
            require(status==200,"GitHub could not provide this update. Please retry later.");break;
        }
        require(input&&input->getStatusCode()==200,"Too many download redirects.");
        require(input->getTotalLength()<0||input->getTotalLength()==release.bytes,"The release download size changed.");
        juce::FileOutputStream output(destination);require(output.openedOk(),"Could not create the downloaded update file.");
        std::array<char,32768> buffer{};
        while(!input->isExhausted()){
            cancelled();require(juce::Time::getMillisecondCounterHiRes()<deadline,"The download timed out. Please retry.");const auto n=input->read(buffer.data(),static_cast<int>(buffer.size()));if(n<=0)break;
            require(received.load()+n<=release.bytes,"The download is larger than its release metadata.");require(output.write(buffer.data(),static_cast<size_t>(n)),"Could not save the update. Check available disk space.");received.fetch_add(n);
        }
        output.flush();cancelled();require(!input->isError()&&received.load()==release.bytes&&output.getStatus().wasOk(),"The download was interrupted. Please retry.");
        {const juce::ScopedLock guard(streamLock);stream.reset();}
    }
   #if JUCE_WINDOWS
    static juce::File bundle()
    {
        HMODULE module=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&bundle),&module))return {};
        std::array<wchar_t,32768> path{};const auto length=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));if(length==0||length>=path.size())return {};
        const juce::File binary(juce::String(path.data()));const auto root=binary.getParentDirectory().getParentDirectory().getParentDirectory();
        return binary.getFileName()=="WUBROT.vst3"&&binary.getParentDirectory().getFileName()=="x86_64-win"&&root.getFileName()=="WUBROT.vst3"?root:juce::File();
    }
    void prepare(const Package& package)
    {
        const auto source=package.target.getChildFile("Contents/Resources/WubrotUpdater.exe"),helper=work.getChildFile("WubrotUpdater.exe");
        require(source.existsAsFile()&&source.copyFileTo(helper)&&hashFile(source)==hashFile(helper),"The installed updater helper could not be prepared.");
        FILETIME start,exit,kernel,user;require(GetProcessTimes(GetCurrentProcess(),&start,&exit,&kernel,&user)!=0,"Could not identify the DAW session.");ULARGE_INTEGER created;created.LowPart=start.dwLowDateTime;created.HighPart=start.dwHighDateTime;
        auto json=std::make_unique<juce::DynamicObject>();json->setProperty("zip",package.zip.getFullPathName());json->setProperty("target",package.target.getFullPathName());json->setProperty("version",release.version);json->setProperty("sha256",release.sha256);json->setProperty("installedSha256",package.installedSha256);json->setProperty("hostPid",static_cast<juce::int64>(GetCurrentProcessId()));json->setProperty("hostCreated",juce::String::toHexString(static_cast<juce::int64>(created.QuadPart)));json->setProperty("helperSha256",hashFile(helper));
        const auto job=work.getChildFile("install.json");require(job.replaceWithText(juce::JSON::toString(juce::var(json.release()),true)),"Could not save the installation request.");
        require(work.getChildFile("cancel.flag").replaceWithText("0"),"Could not prepare update cancellation.");cancelled();const juce::String command="\""+helper.getFullPathName()+"\" --apply \""+job.getFullPathName()+"\" "+hashFile(job)+" --notify";std::wstring args(command.toWideCharPointer());
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};BOOL inJob=FALSE;require(IsProcessInJob(GetCurrentProcess(),nullptr,&inJob)!=0,"Could not check the DAW process environment.");
        const auto flags=CREATE_NO_WINDOW|(inJob?CREATE_BREAKAWAY_FROM_JOB:0);
        require(CreateProcessW(helper.getFullPathName().toWideCharPointer(),args.data(),nullptr,nullptr,FALSE,flags,nullptr,work.getFullPathName().toWideCharPointer(),&startup,&process)!=0,inJob?"This DAW sandbox blocked the updater helper. Run WUBROT without sandboxing to update automatically.":"Windows could not start the updater helper.");
        CloseHandle(process.hThread);helperProcess=process.hProcess;armed.store(true);
    }
   #endif
    void run() override
    {
        try{
           #if JUCE_WINDOWS
            const auto target=bundle();require(target!=juce::File(),"Automatic updates require the installed Windows WUBROT.vst3 bundle.");
            work=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Spock Audio/WUBROT/Updates").getChildFile(juce::Uuid().toString());require(work.createDirectory().wasOk(),"Could not create the update cache.");
            const auto archive=work.getChildFile("update.zip");set("downloading");transfer(archive);cancelled();set("verifying");
            Package package{archive,target,work,release.version,release.sha256,hashFile(moduleFile(target))};require(package.installedSha256.length()==64,"Could not verify the installed plugin.");const auto checked=validateArchive(package);require(checked.wasOk(),checked.getErrorMessage());cancelled();set("staging");prepare(package);if(cancelRequested.load())cancelPrepared();else set("ready");
           #else
            set("failed","Automatic updates are available in Windows VST3 builds.");
           #endif
        }catch(const std::exception& e){set(threadShouldExit()?"cancelled":"failed",juce::String(e.what()));if(!armed.load()&&work!=juce::File())work.deleteRecursively();}
    }
    const Release release;mutable juce::CriticalSection stateLock;juce::String phase="downloading",message;
    std::atomic<juce::int64> received{0};std::atomic<bool> armed{false},cancelRequested{false};juce::File work;
   #if JUCE_WINDOWS
    HANDLE helperProcess=nullptr;
   #endif
    juce::CriticalSection streamLock;std::unique_ptr<juce::WebInputStream> stream;
};
}
Session::Session(juce::String v,std::unique_ptr<Transport> t,bool e,DownloadFactory f) : Thread("WUBROT release check"),installed(std::move(v)),transport(std::move(t)),enabled(e),factory(std::move(f)) {}
Session::~Session()
{
    signalThreadShouldExit();transport->cancel();stopThread(-1);
}
Session& Session::shared(const juce::String& installed)
{
    // One check/notice per loaded plugin module (DAW process session), shared by
    // open instances. Closing an editor does not wait for a network request.
    static Session service(installed,std::make_unique<GitHubTransport>(),juce::SystemStats::getEnvironmentVariable("WUBROT_DISABLE_UPDATE_CHECKS",{})!="1");
    return service;
}
void Session::checkOnce()
{
    if(started.exchange(true))return;
    if(!enabled){done.store(true,std::memory_order_release);return;}
    {const juce::ScopedLock guard(lock);checkPhase="checking";checkMessage="Checking GitHub for a stable update…";}
    if(!startThread()){const juce::ScopedLock guard(lock);checkPhase="failed";checkMessage="Could not start the update check. Please retry.";done.store(true,std::memory_order_release);}
}
void Session::checkNow()
{
    const juce::ScopedLock guard(lock);
    if(isThreadRunning())return;
    if(owner||(task&&task->snapshot()["phase"].toString()!="failed"&&task->snapshot()["phase"].toString()!="cancelled")){checkPhase="busy";checkMessage="Finish or close the current update menu before checking again.";return;}
    task.reset();
    started.store(true);done.store(false,std::memory_order_release);checkPhase="checking";checkMessage="Checking GitHub for a stable update…";
    if(!startThread()){checkPhase="failed";checkMessage="Could not start the update check. Please retry.";done.store(true,std::memory_order_release);}
}
juce::var Session::checkStatus()
{
    const juce::ScopedLock guard(lock);auto data=std::make_unique<juce::DynamicObject>();data->setProperty("phase",checkPhase);data->setProperty("message",checkMessage);data->setProperty("installed",installed);data->setProperty("revision",checkRevision);return juce::var(data.release());
}
void Session::run()
{
    std::optional<Release> candidate;juce::String phase="failed",message="Could not check for updates. Check your connection and try again.";
    try{
        const auto body=transport->fetch();candidate=parseRelease(body,installed);
        if(candidate){phase="available";message="WUBROT v"+candidate->version+" is available.";}
        else if(transport->statusCode()==404){phase="current";message="No stable releases have been published yet.";}
        else if(body.isNotEmpty()&&transport->statusCode()==200){
            const auto json=juce::JSON::parse(body);const auto latest=version(json["tag_name"].toString()),current=version(installed);
            if(latest&&current&&json["draft"].isBool()&&!bool(json["draft"])&&json["prerelease"].isBool()&&!bool(json["prerelease"])&&!latest->prerelease){phase="current";message=(latest->numbers>current->numbers||(latest->numbers==current->numbers&&current->prerelease))?"The latest release has no compatible Windows update package yet.":"You’re up to date. No newer stable update was found.";}
        }
    }catch(...){ }
    {const juce::ScopedLock guard(lock);if(!threadShouldExit()){result=std::move(candidate);checkPhase=phase;checkMessage=message;++checkRevision;owner=nullptr;announced=false;downloadDismissed=false;}}
    done.store(true,std::memory_order_release);
}
bool Session::owns(const void* editor,const juce::String& v) const {return owner==editor && result && result->version==v;}
juce::var Session::claim(const void* editor)
{
    if(!finished()) return {};
    const juce::ScopedLock guard(lock);
    const auto progress=task?task->snapshot():juce::var();const auto phase=progress["phase"].toString();const bool active=phase=="downloading"||phase=="verifying"||phase=="staging"||phase=="cancelling"||phase=="ready";
    if(result&&announced&&downloadDismissed){auto notice=result->notice(installed);notice.getDynamicObject()->setProperty("phase","minimized");return notice;}
    if(!result || (announced&&(!active||downloadDismissed)) || (owner && owner!=editor)) return {};
    owner=editor;auto notice=result->notice(installed);if(task){const auto state=task->snapshot();const auto& props=state.getDynamicObject()->getProperties();for(int i=0;i<props.size();++i)notice.getDynamicObject()->setProperty(props.getName(i),props.getValueAt(i));}return notice;
}
void Session::release(const void* editor) {const juce::ScopedLock guard(lock);if(owner==editor)owner=nullptr;}
void Session::shown(const void* editor,const juce::String& v) {const juce::ScopedLock guard(lock);if(owns(editor,v))announced=true;}
void Session::later(const void* editor,const juce::String& v) {const juce::ScopedLock guard(lock);if(owns(editor,v)){announced=true;downloadDismissed=true;owner=nullptr;}}
juce::var Session::reopen(const void* editor,const juce::String& v)
{
    const juce::ScopedLock guard(lock);if(!finished()||!result||result->version!=v||(owner&&owner!=editor))return {};
    owner=editor;downloadDismissed=false;auto notice=result->notice(installed);notice.getDynamicObject()->setProperty("reopened",true);
    if(task){const auto state=task->snapshot();if(state["phase"].toString()=="cancelled")task.reset();else if(const auto* data=state.getDynamicObject()){const auto& props=data->getProperties();for(int i=0;i<props.size();++i)notice.getDynamicObject()->setProperty(props.getName(i),props.getValueAt(i));}}
    return notice;
}
juce::Result Session::download(const void* editor,const juce::String& v)
{
    const juce::ScopedLock guard(lock);if(!owns(editor,v))return juce::Result::fail("This update is no longer available.");
    if(task){const auto phase=task->snapshot()["phase"].toString();if(phase!="failed"&&phase!="cancelled")return juce::Result::fail("An update is already in progress.");}
    task=factory?factory(*result):std::make_unique<NativeDownload>(*result);downloadDismissed=false;task->start();return juce::Result::ok();
}
void Session::cancel(const void* editor,const juce::String& v){const juce::ScopedLock guard(lock);if(owns(editor,v)&&task)task->cancel();}
juce::var Session::progress(const void* editor){const juce::ScopedLock guard(lock);return owner==editor&&task?task->snapshot():juce::var();}
juce::String Session::link(const void* editor,const juce::String& v,bool notes)
{
    const juce::ScopedLock guard(lock);return owns(editor,v)?(notes?result->page:result->download):juce::String();
}
}
