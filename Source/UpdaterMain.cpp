// A separate process applies the verified bundle only after the originating DAW
// has exited. It never kills a host or modifies a mapped plugin in place.
#include "UpdatePackage.h"
#include <windows.h>
#include <shellapi.h>
#include <restartmanager.h>
#include <vector>
#include <juce_cryptography/juce_cryptography.h>

namespace
{
struct Handle
{
    HANDLE value=nullptr;
    explicit Handle(HANDLE v=nullptr):value(v){}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    ~Handle(){reset();}
    void reset(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=nullptr;}
    bool valid() const {return value&&value!=INVALID_HANDLE_VALUE;}
};
Handle denyWrites(const juce::File& file)
{
    return Handle(CreateFileW(file.getFullPathName().toWideCharPointer(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
}
juce::String created(HANDLE process)
{
    FILETIME start,exit,kernel,user;if(!GetProcessTimes(process,&start,&exit,&kernel,&user))return {};
    ULARGE_INTEGER n;n.LowPart=start.dwLowDateTime;n.HighPart=start.dwHighDateTime;return juce::String::toHexString(static_cast<juce::int64>(n.QuadPart));
}
bool unused(const juce::File& module)
{
    DWORD session=0;WCHAR key[CCH_RM_SESSION_KEY+1]{};
    if(RmStartSession(&session,0,key)!=ERROR_SUCCESS)return false;
    struct End{DWORD value;~End(){RmEndSession(value);}}end{session};
    const auto path=module.getFullPathName();LPCWSTR files[]{path.toWideCharPointer()};
    if(RmRegisterResources(session,1,files,0,nullptr,0,nullptr)!=ERROR_SUCCESS)return false;
    UINT needed=0,count=0;DWORD reason=0;
    const auto status=RmGetList(session,&needed,&count,nullptr,&reason);
    return status==ERROR_SUCCESS&&needed==0;
}
void report(const juce::File& work,const juce::String& phase,const juce::String& message={})
{
    auto data=std::make_unique<juce::DynamicObject>();data->setProperty("phase",phase);data->setProperty("message",message);
    work.getChildFile("install-status.json").replaceWithText(juce::JSON::toString(juce::var(data.release()),true));
}
int apply(const juce::StringArray& args)
{
    if(args.size()<4||args[1]!="--apply")return 2;
    const juce::File jobFile(args[2]);const auto expected=args[3];
    Handle jobLock=denyWrites(jobFile);if(!jobLock.valid()||WubrotUpdates::hashFile(jobFile)!=expected||jobFile.getSize()>32768)return 3;
    const auto data=juce::JSON::parse(jobFile.loadFileAsString());if(!data.isObject())return 3;
    const auto fingerprint=[](const juce::var& v){return v.isString()&&v.toString().length()==64&&v.toString().containsOnly("0123456789abcdef");};
    const auto version=data["version"].toString(),core=version.upToFirstOccurrenceOf("+",false,false);const auto numbers=juce::StringArray::fromTokens(core,".","");
    if(!data["zip"].isString()||!data["target"].isString()||!juce::File::isAbsolutePath(data["zip"].toString())||!juce::File::isAbsolutePath(data["target"].toString())||!fingerprint(data["sha256"])||!fingerprint(data["installedSha256"])||!fingerprint(data["helperSha256"])||numbers.size()!=3||numbers.joinIntoString(".")!=core)return 3;
    for(const auto& n:numbers)if(n.isEmpty()||n.length()>5||!n.containsOnly("0123456789")||n.getIntValue()>65535||(n.length()>1&&n.startsWithChar('0')))return 3;
    if((!data["hostPid"].isInt()&&!data["hostPid"].isInt64())||static_cast<juce::int64>(data["hostPid"])<=0||static_cast<juce::int64>(data["hostPid"])>4294967295LL)return 3;
    const auto hostCreated=data["hostCreated"].toString();if(hostCreated.isEmpty()||hostCreated.length()>16||!hostCreated.containsOnly("0123456789abcdef")||hostCreated.getHexValue64()==0)return 3;
    WubrotUpdates::Package p{juce::File(data["zip"].toString()),juce::File(data["target"].toString()),jobFile.getParentDirectory(),data["version"].toString(),data["sha256"].toString(),data["installedSha256"].toString()};
    if(p.target.getFileName()!="WUBROT.vst3"||!p.target.isDirectory()||p.target.isSymbolicLink()||!p.zip.isAChildOf(p.work))return 3;
    const auto self=juce::File::getSpecialLocation(juce::File::currentExecutableFile);Handle selfLock=denyWrites(self);
    if(!selfLock.valid()||WubrotUpdates::hashFile(self)!=data["helperSha256"].toString())return 3;
    const bool notify=args.contains("--notify"),elevated=args.contains("--elevated");
    auto fail=[&](const juce::String& message){report(p.work,"failed",message);if(notify)MessageBoxW(nullptr,(message+"\n\nYour previous version is preserved.").toWideCharPointer(),L"WUBROT update",MB_OK|MB_ICONWARNING);return 1;};
    const auto cancelled=[&]{return p.work.getChildFile("cancel.flag").loadFileAsString().startsWithChar('1');};
    if(cancelled()){report(p.work,"cancelled");return 0;}
    // A process handle plus creation time prevents waiting on a reused PID.
    const auto pid=static_cast<DWORD>(static_cast<juce::int64>(data["hostPid"]));
    Handle host(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
    if(!host.valid()&&GetLastError()!=ERROR_INVALID_PARAMETER)return fail("Could not confirm that the DAW has closed.");
    report(p.work,"waiting-for-daw");
    if(host.valid()){
        const auto actual=created(host.value);if(actual.isEmpty())return fail("Could not verify the DAW session.");
        if(actual==data["hostCreated"].toString())for(;;){const auto wait=WaitForSingleObject(host.value,250);if(wait==WAIT_OBJECT_0)break;if(wait!=WAIT_TIMEOUT)return fail("Could not wait for the DAW to close.");if(cancelled()){report(p.work,"cancelled");return 0;}}
    }
    if(cancelled()){report(p.work,"cancelled");return 0;}
    // Serialize by target path, not by release/job, across host processes.
    const auto targetKey=juce::String::toHexString(p.target.getFullPathName().toLowerCase().hashCode64());
    Handle mutex(CreateMutexW(nullptr,FALSE,("Local\\WUBROT-Update-"+targetKey).toWideCharPointer()));
    if(!mutex.valid())return fail("Could not lock the WUBROT installation.");
    for(;;){const auto wait=WaitForSingleObject(mutex.value,250);if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED)break;if(wait!=WAIT_TIMEOUT)return fail("Could not lock the WUBROT installation.");if(cancelled()){report(p.work,"cancelled");return 0;}}
    struct Unlock{HANDLE handle;~Unlock(){ReleaseMutex(handle);}}unlock{mutex.value};
    if(WubrotUpdates::validateModule(WubrotUpdates::moduleFile(p.target),p.version).wasOk()){report(p.work,"completed","This version is already installed.");return 0;}
    const auto probe=p.target.getSiblingFile(".wubrot-permission-"+juce::Uuid().toString());const auto writable=probe.createDirectory();probe.deleteRecursively();
    if(writable.failed()){
        if(elevated)return fail("The installation folder is not writable.");
        const juce::String parameters="--apply \""+jobFile.getFullPathName()+"\" "+expected+" --elevated"+(notify?" --notify":"");
        SHELLEXECUTEINFOW execute{};execute.cbSize=sizeof(execute);execute.fMask=SEE_MASK_NOCLOSEPROCESS;execute.lpVerb=L"runas";const auto path=self.getFullPathName();execute.lpFile=path.toWideCharPointer();execute.lpParameters=parameters.toWideCharPointer();execute.nShow=SW_HIDE;
        // The executing helper and job remain write/delete-locked across elevation.
        if(!ShellExecuteExW(&execute))return fail("Windows did not allow automatic installation.");if(execute.hProcess)CloseHandle(execute.hProcess);return 0;
    }
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+3600000.;
    while(!unused(WubrotUpdates::moduleFile(p.target))){if(cancelled()){report(p.work,"cancelled");return 0;}if(juce::Time::getMillisecondCounterHiRes()>deadline)return fail("Another application is still using WUBROT. Close it and retry the update.");Sleep(500);}
    if(cancelled()){report(p.work,"cancelled");return 0;}
    const auto verifyImage=[&]()->juce::Result{
        // Lock against writes while checking the exact installed image. Close
        // this handle before renaming its parent: Windows forbids that rename
        // with an open child handle, even when FILE_SHARE_DELETE is specified.
        Handle image=denyWrites(WubrotUpdates::moduleFile(p.target));
        if(!image.valid())return juce::Result::fail("WUBROT is still in use by another application.");
        if(WubrotUpdates::hashFile(WubrotUpdates::moduleFile(p.target))!=p.installedSha256)return juce::Result::fail("The installed plugin changed. No files were replaced.");
        return juce::Result::ok();};
    Handle archive=denyWrites(p.zip);if(!archive.valid())return fail("Cannot open the downloaded update.");
    report(p.work,"installing");const auto result=WubrotUpdates::applyPackage(p,{}, {},verifyImage);if(result.failed())return fail(result.getErrorMessage());
    report(p.work,"completed");archive.reset();p.zip.deleteFile();if(notify)MessageBoxW(nullptr,("WUBROT has been updated to v"+p.version+". You can reopen your DAW.").toWideCharPointer(),L"WUBROT update complete",MB_OK|MB_ICONINFORMATION);return 0;
}
}
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int)
{
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);juce::StringArray args;
    if(!argv)return 2;for(int i=0;i<argc;++i)args.add(juce::String(argv[i]));LocalFree(argv);
    return apply(args);
}
