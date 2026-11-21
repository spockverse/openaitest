#include "UpdatePackage.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <utility>
using namespace WubrotUpdates;
static void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Process
{
    HANDLE handle=nullptr;DWORD pid=0;
    Process(HANDLE h,DWORD p):handle(h),pid(p){}
    Process(const Process&)=delete;Process& operator=(const Process&)=delete;
    ~Process(){if(handle)CloseHandle(handle);}
    bool running()const{return WaitForSingleObject(handle,0)==WAIT_TIMEOUT;}
    DWORD wait(){need(WaitForSingleObject(handle,20000)==WAIT_OBJECT_0,"Child process timed out");DWORD code=99;GetExitCodeProcess(handle,&code);return code;}
};
static Process launch(const juce::File& exe,const juce::StringArray& args)
{
    juce::String text="\""+exe.getFullPathName()+"\"";for(const auto& arg:args)text+=" \""+arg+"\"";std::wstring command(text.toWideCharPointer());STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    need(CreateProcessW(exe.getFullPathName().toWideCharPointer(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=0,"Could not start child process");CloseHandle(child.hThread);return Process(child.hProcess,child.dwProcessId);
}
static juce::String created(HANDLE p){FILETIME c,e,k,u;need(GetProcessTimes(p,&c,&e,&k,&u)!=0,"Cannot read host creation time");ULARGE_INTEGER n;n.LowPart=c.dwLowDateTime;n.HighPart=c.dwHighDateTime;return juce::String::toHexString(static_cast<juce::int64>(n.QuadPart));}
static void waitFile(const juce::File& f){for(int i=0;i<200&&!f.existsAsFile();++i)Sleep(25);need(f.existsAsFile(),"DLL holder did not start");}
int main(int argc,char** argv)
{
 if(argc>1&&juce::String(argv[1])=="--hold"){
    const juce::File dll(argv[2]),ready(argv[3]),stop(argv[4]);const auto module=LoadLibraryW(dll.getFullPathName().toWideCharPointer());if(!module)return 3;ready.replaceWithText("ready");for(int i=0;i<1200&&!stop.existsAsFile();++i)Sleep(25);FreeLibrary(module);return 0;
 }
 try{
    need(argc==4,"Expected helper, built bundle, and old fixture paths");const juce::File helperSource(argv[1]),bundle(argv[2]),fixture(argv[3]);const auto self=juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const auto root=juce::File::createTempFile("-WubrotWindowsUpdate");root.deleteFile();root.createDirectory();struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}}cleanup{root};
    const auto archive=root.getChildFile("release.zip");{
        juce::ZipFile::Builder builder;for(const auto& file:bundle.findChildFiles(juce::File::findFiles,true,"*"))builder.addFile(file,6,"WUBROT.vst3/"+file.getRelativePathFrom(bundle).replaceCharacter('\\','/'));
        juce::FileOutputStream stream(archive);need(stream.openedOk()&&builder.writeToStream(stream,nullptr),"Cannot build release ZIP");
    }
    for(int test=0;test<3;++test){
        const auto base=root.getChildFile("case-"+juce::String(test)),target=base.getChildFile("install/WUBROT.vst3"),work=base.getChildFile("cache");moduleFile(target).getParentDirectory().createDirectory();need(fixture.copyFileTo(moduleFile(target)),"Cannot install old fixture");work.createDirectory();
        const auto zip=work.getChildFile("update.zip"),helper=work.getChildFile("WubrotUpdater.exe"),ready=base.getChildFile("holder.ready"),stop=base.getChildFile("holder.stop");need(archive.copyFileTo(zip)&&helperSource.copyFileTo(helper),"Cannot prepare helper");
        auto host=launch(self,{"--hold",moduleFile(target).getFullPathName(),ready.getFullPathName(),stop.getFullPathName()});waitFile(ready);
        struct Stop{juce::File file;HANDLE process;~Stop(){file.replaceWithText("stop");WaitForSingleObject(process,5000);}}stopper{stop,host.handle};
        const auto oldHash=hashFile(moduleFile(target));auto job=std::make_unique<juce::DynamicObject>();job->setProperty("target",target.getFullPathName());job->setProperty("zip",zip.getFullPathName());job->setProperty("version",WUBROT_TEST_VERSION);job->setProperty("sha256",test==2?juce::String::repeatedString("0",64):hashFile(zip));job->setProperty("installedSha256",oldHash);job->setProperty("hostPid",static_cast<juce::int64>(host.pid));job->setProperty("hostCreated",created(host.handle));job->setProperty("helperSha256",hashFile(helper));
        const auto file=work.getChildFile("install.json");need(file.replaceWithText(juce::JSON::toString(juce::var(job.release()),true)),"Cannot save job");work.getChildFile("cancel.flag").replaceWithText("0");
        auto updater=launch(helper,{"--apply",file.getFullPathName(),hashFile(file)});const auto status=work.getChildFile("install-status.json");
        for(int i=0;i<200&&juce::JSON::parse(status.loadFileAsString())["phase"].toString()!="waiting-for-daw";++i)Sleep(25);
        need(updater.running()&&host.running()&&hashFile(moduleFile(target))==oldHash,"Updater changed or terminated a running host");
        if(test==1){work.getChildFile("cancel.flag").replaceWithText("1");need(updater.wait()==0&&host.running()&&hashFile(moduleFile(target))==oldHash,"Queued update cancellation failed");}
        else{stop.replaceWithText("stop");need(host.wait()==0,"DLL holder failed");const auto result=updater.wait();
            if(test==0){if(result!=0)std::cerr<<"Helper exit="<<result<<" status="<<status.loadFileAsString()<<'\n';need(result==0,"Real Windows helper install failed");need(validateModule(moduleFile(target),WUBROT_TEST_VERSION).wasOk(),"Wrong installed version");need(hashFile(moduleFile(work.getChildFile("rollback/WUBROT.vst3")))==oldHash,"Real helper lost rollback copy");}
            else need(result!=0&&hashFile(moduleFile(target))==oldHash,"Tampered ZIP replaced installed files");
        }
    }
    std::cout<<"PASS actual updater process: held DLL/session waits, verified full-bundle install, cancellation, tamper rejection and preserved rollback\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
