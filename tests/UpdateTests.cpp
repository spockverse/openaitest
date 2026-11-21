#include "UpdateChecker.h"
#include "UpdatePackage.h"
#include <iostream>
#include <thread>
#include <stdexcept>
using namespace WubrotUpdates;
static void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static juce::String fixture(const juce::String& tag="v1.2.0")
{
    return "{\"tag_name\":\""+tag+"\",\"draft\":false,\"prerelease\":false,\"body\":\"New textures and fixes.\",\"assets\":[{\"name\":\"WUBROT-1.2.0-Windows-x64.zip\",\"state\":\"uploaded\",\"size\":100,\"digest\":\"sha256:"+juce::String::repeatedString("a",64)+"\",\"browser_download_url\":\"https://evil.example/ignored.exe\"}]}";
}
struct Fake final:Transport
{
    std::atomic<int> calls{0};std::atomic<bool> cancelled{false},block{false};juce::String body=fixture();std::thread::id thread;int code=200;
    juce::String fetch()override{++calls;thread=std::this_thread::get_id();while(block&&!cancelled)juce::Thread::sleep(1);return body;}
    void cancel()override{cancelled=true;}
    int statusCode()const override{return code;}
};
struct Download final:DownloadTask
{
    int starts=0,cancels=0;juce::String phase="downloading";
    void start()override{++starts;}void cancel()override{++cancels;phase="cancelled";}
    juce::var snapshot()const override{auto d=std::make_unique<juce::DynamicObject>();d->setProperty("phase",phase);d->setProperty("received",60);d->setProperty("total",100);return juce::var(d.release());}
};
static void wait(Session& s){const auto deadline=juce::Time::getMillisecondCounter()+2000;while(!s.finished()&&juce::Time::getMillisecondCounter()<deadline)juce::Thread::sleep(1);need(s.finished(),"Release check did not finish");}
static void zip(const juce::File& destination,const juce::String& extra={})
{
    juce::ZipFile::Builder builder;for(const auto& name:juce::StringArray{"WUBROT.vst3/Contents/x86_64-win/WUBROT.vst3","WUBROT.vst3/Contents/Resources/WubrotUpdater.exe"})builder.addEntry(new juce::MemoryInputStream("new",3,false),6,name,juce::Time::getCurrentTime());
    if(extra.isNotEmpty())builder.addEntry(new juce::MemoryInputStream("bad",3,false),6,extra,juce::Time::getCurrentTime());
    juce::FileOutputStream stream(destination);need(stream.openedOk()&&builder.writeToStream(stream,nullptr),"Cannot write archive fixture");
}
int main()
{
 try{
    const auto good=parseRelease(fixture(),"1.1.9");need(bool(good),"New stable release missing");need(good->download.startsWith("https://github.com/spockverse/openaitest/releases/download/v1.2.0/"),"Untrusted asset URL escaped policy");
    for(const auto& installed:juce::StringArray{"1.2.0","1.2.1","2.0.0","bad"})need(!parseRelease(fixture(),installed),"Equal/older release offered");
    need(bool(parseRelease(fixture("v1.10.0"),"1.9.99")),"Versions compared lexically");need(bool(parseRelease(fixture(),"1.2.0-beta.1")),"Stable upgrade from beta missing");
    for(const auto& tag:juce::StringArray{"v1.2","v01.2.3","v1.2.3.4","v1.2.3-beta","v1.2.99999999999","v1.2.65536","v1.2.3/evil","v1.2.3+bad..metadata","v1.2.3.","v1..2"})need(!parseRelease(fixture(tag),"0.1.0"),"Malformed/prerelease version offered");
    need(!parseRelease(fixture().replace("\"draft\":false","\"draft\":true"),"0.1.0"),"Draft offered");need(!parseRelease(fixture().replace("\"prerelease\":false","\"prerelease\":true"),"0.1.0"),"Prerelease offered");
    need(!parseRelease(fixture().replace("sha256:","md5:"),"0.1.0"),"Missing digest accepted");need(!parseRelease(fixture().replace("\"size\":100","\"size\":134217729"),"0.1.0"),"Oversized ZIP offered");
    need(!parseRelease("not json","0.1.0")&&!parseRelease("{\"message\":\"rate limit\"}","0.1.0"),"Network errors offered as updates");
    auto fetch=std::make_unique<Fake>();auto* f=fetch.get();Download* download=nullptr;
    Session session("1.0.0",std::move(fetch),true,[&](const Release& release){need(release.bytes==100,"Release metadata lost");auto d=std::make_unique<Download>();download=d.get();return d;});
    std::vector<std::thread> requests;for(int i=0;i<8;++i)requests.emplace_back([&]{session.checkOnce();});for(auto& t:requests)t.join();wait(session);need(f->calls==1&&f->thread!=std::this_thread::get_id(),"Check not deduplicated/off thread");
    int a=0,b=0;auto notice=session.claim(&a);need(notice["version"].toString()=="1.2.0"&&!session.claim(&b).isObject(),"Multiple editors claimed one notice");session.release(&a);need(session.claim(&b).isObject(),"Unshown notice lost when editor closed");session.shown(&b,"1.2.0");
    need(session.download(&a,"1.2.0").failed(),"Wrong editor could start download");need(session.download(&b,"0.0.1").failed(),"Wrong version could start download");need(session.download(&b,"1.2.0").wasOk()&&download->starts==1,"Download did not start");
    need(session.download(&b,"1.2.0").failed(),"Duplicate download started");session.cancel(&b,"1.2.0");need(download->cancels==1&&session.progress(&b)["phase"].toString()=="cancelled","Cancellation missing");session.later(&b,"1.2.0");need(session.claim(&a)["phase"].toString()=="minimized","Later did not retain a minimized notice");
    need(!session.reopen(&a,"9.9.9").isObject(),"Badge accepted a stale version");need(bool(session.reopen(&a,"1.2.0")["reopened"]),"Minimized notice could not reopen");need(!session.reopen(&b,"1.2.0").isObject(),"Two editors reopened one update menu");need(session.download(&a,"1.2.0").wasOk(),"Reopened update lost download ownership");session.cancel(&a,"1.2.0");session.later(&a,"1.2.0");session.release(&a);need(session.claim(&b)["phase"].toString()=="minimized","Editor reopen lost minimized notice");
    f->block=true;session.checkNow();need(!session.reopen(&a,"1.2.0").isObject(),"Badge reopened stale metadata during a manual refresh");f->block=false;wait(session);
    {auto blocked=std::make_unique<Fake>();blocked->block=true;Session closing("1.0.0",std::move(blocked));closing.checkOnce();} // Cancellation must release/join the worker.
    {auto offline=std::make_unique<Fake>();offline->body={};Session quiet("1.0.0",std::move(offline));quiet.checkOnce();wait(quiet);need(!quiet.claim(&a).isObject(),"Offline check interrupted UI");}
    {auto backend=std::make_unique<Fake>();auto* source=backend.get();source->body=fixture("v1.0.0");Session manual("1.0.0",std::move(backend));manual.checkOnce();wait(manual);need(manual.checkStatus()["phase"].toString()=="current","Installed version incorrectly offered as an update");
        while(!manual.finished())juce::Thread::sleep(1);juce::Thread::sleep(2);source->body={};source->code=0;source->block=true;manual.checkNow();manual.checkNow();source->block=false;wait(manual);need(source->calls==2&&manual.checkStatus()["phase"].toString()=="failed","Manual check failure or deduplication incorrect");
        juce::Thread::sleep(2);source->body=fixture("v1.3.0");source->code=200;manual.checkNow();wait(manual);need(manual.claim(&a)["version"].toString()=="1.3.0","Manual retry did not recover and find a newer release");manual.later(&a,"1.3.0");
        juce::Thread::sleep(2);source->body={};source->code=404;manual.checkNow();wait(manual);need(manual.checkStatus()["phase"].toString()=="current"&&!manual.claim(&a).isObject(),"No published releases incorrectly offered an update");}
    const auto root=juce::File::createTempFile("-WubrotUpdates");root.deleteFile();root.createDirectory();struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}}cleanup{root};
    auto make=[&](juce::String extra={}){const auto work=root.getChildFile(juce::Uuid().toString());work.createDirectory();const auto target=root.getChildFile("installed").getChildFile("WUBROT.vst3");target.deleteRecursively();moduleFile(target).getParentDirectory().createDirectory();moduleFile(target).replaceWithText("old");const auto archive=work.getChildFile("update.zip");zip(archive,extra);return Package{archive,target,work,"1.2.0",hashFile(archive),hashFile(moduleFile(target))};};
    auto p=make();need(validateArchive(p).wasOk(),"Valid ZIP rejected");auto validate=[](const juce::File& f,const juce::String&){return f.loadFileAsString()=="new"?juce::Result::ok():juce::Result::fail("bad module");};
    need(applyPackage(p,{},validate).wasOk()&&moduleFile(p.target).loadFileAsString()=="new","Bundle install failed");need(moduleFile(p.work.getChildFile("rollback/WUBROT.vst3")).loadFileAsString()=="old","Rollback copy missing");
    p=make();int moves=0;const auto failed=applyPackage(p,[&](const juce::File& a,const juce::File& b){if(++moves==2)return false;return a.moveFileTo(b);},validate);need(failed.failed()&&moduleFile(p.target).loadFileAsString()=="old","Failed commit did not restore old bundle");
    p=make();p.sha256=juce::String::repeatedString("0",64);need(applyPackage(p,{},validate).failed()&&moduleFile(p.target).loadFileAsString()=="old","Bad hash changed installation");
    for(const auto& path:juce::StringArray{"../escape","WUBROT.vst3/../../escape","WUBROT.vst3/Contents/../escape","WUBROT.vst3/C:/escape","WUBROT.vst3/Contents//bad","WUBROT.vst3/Contents/bad."}){p=make(path);need(validateArchive(p).failed(),"Unsafe ZIP path accepted");}
    p=make();moduleFile(p.target).replaceWithText("changed");need(applyPackage(p,{},validate).failed()&&moduleFile(p.target).loadFileAsString()=="changed","Changed installation overwritten");
    p=make();auto manifest=p.target.getChildFile("Contents/Resources/moduleinfo.json");manifest.getParentDirectory().createDirectory();manifest.replaceWithText("{\"Classes\":[{\"CID\":\"0123456789abcdef0123456789abcdef\"}]}");need(applyPackage(p,{},validate).failed()&&moduleFile(p.target).loadFileAsString()=="old","Changed plugin identity accepted");
    p=make();need(applyPackage(p,{},[](const juce::File&,const juce::String&){return juce::Result::fail("wrong product");}).failed()&&moduleFile(p.target).loadFileAsString()=="old","Invalid product replaced installation");
    p=make();need(applyPackage(p,{},validate,[]{return juce::Result::fail("held image");}).failed()&&moduleFile(p.target).loadFileAsString()=="old","Held image replaced installation");
    std::cout<<"PASS stable-version policy, trusted URLs/digests, session ownership, worker cancellation, package validation, atomic install and rollback\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
