#include "UpdatePackage.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cstdio>
#include <array>
#if JUCE_WINDOWS
#include <windows.h>
#include <vector>
#endif
namespace WubrotUpdates
{
juce::String hashFile(const juce::File& file)
{
    juce::FileInputStream stream(file);if(!stream.openedOk())return {};
    return juce::SHA256(stream).toHexString();
}
juce::File moduleFile(const juce::File& root) {return root.getChildFile("Contents/x86_64-win/WUBROT.vst3");}
juce::Result validateArchive(const Package& package)
{
    if(package.sha256.length()!=64 || !package.sha256.containsOnly("0123456789abcdefABCDEF") || !package.zip.existsAsFile() || package.zip.getSize()>128*1024*1024 || hashFile(package.zip)!=package.sha256.toLowerCase())return juce::Result::fail("The downloaded update failed its SHA-256 check.");
    juce::ZipFile zip(package.zip);const int count=zip.getNumEntries();if(count<2||count>4096)return juce::Result::fail("The update archive is invalid.");
    juce::int64 total=0;bool module=false,helper=false;juce::StringArray names;
    for(int i=0;i<count;++i){const auto* entry=zip.getEntry(i);const auto name=entry->filename;
        if(entry->isSymbolicLink || name.length()>400 || name.containsAnyOf("\\:") || name.contains("//") || name.startsWithChar('/') || !name.startsWith("WUBROT.vst3/") || entry->uncompressedSize<0 || entry->uncompressedSize>512*1024*1024)return juce::Result::fail("The update archive contains an unsafe entry.");
        auto parts=juce::StringArray::fromTokens(name,"/","");for(const auto& part:parts)if(part==".."||part=="."||part.endsWithChar('.')||part.endsWithChar(' '))return juce::Result::fail("The update archive contains an unsafe path.");
        const auto key=name.toLowerCase();if(names.contains(key))return juce::Result::fail("The update archive contains duplicate paths.");names.add(key);
        for(const auto& part:parts){const auto base=part.upToFirstOccurrenceOf(".",false,false).toUpperCase();if(base=="CON"||base=="PRN"||base=="AUX"||base=="NUL"||(base.length()==4&&(base.startsWith("COM")||base.startsWith("LPT"))&&juce::String("123456789").containsChar(base[3])))return juce::Result::fail("The update archive contains a reserved Windows filename.");}
        total+=entry->uncompressedSize;if(total>512*1024*1024)return juce::Result::fail("The unpacked update is too large.");
        module|=name=="WUBROT.vst3/Contents/x86_64-win/WUBROT.vst3";helper|=name=="WUBROT.vst3/Contents/Resources/WubrotUpdater.exe";
    }
    return module&&helper?juce::Result::ok():juce::Result::fail("This release is missing the Windows plugin or updater.");
}
juce::Result validateModule(const juce::File& file,const juce::String& version)
{
   #if JUCE_WINDOWS
    const auto actual=juce::StringArray::fromTokens(file.getVersion(),".","");
    const auto wanted=juce::StringArray::fromTokens(version.upToFirstOccurrenceOf("+",false,false),".","");
    if(actual.size()<3||wanted.size()!=3)return juce::Result::fail("The update has no valid Windows version information.");
    for(int i=0;i<3;++i)if(actual[i].getIntValue()!=wanted[i].getIntValue())return juce::Result::fail("The plugin version does not match its release tag.");
    DWORD ignored=0;const auto size=GetFileVersionInfoSizeW(file.getFullPathName().toWideCharPointer(),&ignored);if(!size)return juce::Result::fail("The update has no product identity.");
    std::vector<unsigned char> data(size);if(!GetFileVersionInfoW(file.getFullPathName().toWideCharPointer(),0,size,data.data()))return juce::Result::fail("Cannot read update identity.");
    struct Translation{WORD language,codePage;};Translation* translations=nullptr;UINT bytes=0;
    if(!VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&translations),&bytes)||bytes<sizeof(Translation))return juce::Result::fail("The update has no product identity.");
    const auto key=juce::String::formatted("\\StringFileInfo\\%04x%04x\\ProductName",translations[0].language,translations[0].codePage);wchar_t* product=nullptr;UINT chars=0;
    if(!VerQueryValueW(data.data(),key.toWideCharPointer(),reinterpret_cast<void**>(&product),&chars)||juce::String(product).trim()!="WUBROT")return juce::Result::fail("This package is not WUBROT.");
    return juce::Result::ok();
   #else
    juce::ignoreUnused(file,version);return juce::Result::fail("Automatic installation is supported by Windows VST3 builds.");
   #endif
}
juce::Result applyPackage(const Package& p,const std::function<bool(const juce::File&,const juce::File&)>& mover,const std::function<juce::Result(const juce::File&,const juce::String&)>& validator,const std::function<juce::Result()>& beforeSwap,const std::function<void()>& afterSwap)
{
    if(p.installedSha256.length()!=64||!p.installedSha256.containsOnly("0123456789abcdefABCDEF"))return juce::Result::fail("The installed plugin fingerprint is invalid.");
    if(p.work==p.target||p.work.isAChildOf(p.target)||p.target.isAChildOf(p.work)||!p.zip.isAChildOf(p.work))return juce::Result::fail("Invalid update staging location.");
    if(p.target.getFileName()!="WUBROT.vst3"||!p.target.isDirectory()||p.target.isSymbolicLink()||!moduleFile(p.target).existsAsFile())return juce::Result::fail("The installed WUBROT bundle could not be identified.");
    if(hashFile(moduleFile(p.target))!=p.installedSha256)return juce::Result::fail("The installed plugin changed after this update was requested. No files were replaced.");
    const auto archive=validateArchive(p);if(archive.failed())return archive;
    const auto stage=p.target.getSiblingFile(".wubrot-stage-"+juce::Uuid().toString()),old=p.target.getSiblingFile(".wubrot-previous-"+juce::Uuid().toString());
    auto created=stage.createDirectory();if(created.failed())return created;
    struct Cleanup{juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{stage};
    juce::ZipFile zip(p.zip);juce::int64 unpacked=0;std::array<char,32768> buffer{};
    for(int i=0;i<zip.getNumEntries();++i){
        const auto* entry=zip.getEntry(i);const auto destination=stage.getChildFile(entry->filename);
        if(entry->filename.endsWithChar('/')){if(destination.createDirectory().failed())return juce::Result::fail("Could not create update folders.");continue;}
        if(destination.getParentDirectory().createDirectory().failed())return juce::Result::fail("Could not create update folders.");
        std::unique_ptr<juce::InputStream> input(zip.createStreamForEntry(i));juce::FileOutputStream output(destination);if(!input||!output.openedOk())return juce::Result::fail("Could not unpack the update.");
        juce::int64 size=0;for(;;){const auto n=input->read(buffer.data(),static_cast<int>(buffer.size()));if(n<=0)break;size+=n;unpacked+=n;if(size>entry->uncompressedSize||unpacked>512*1024*1024||!output.write(buffer.data(),static_cast<size_t>(n)))return juce::Result::fail("The unpacked update exceeded its declared size or could not be saved.");}
        output.flush();if(size!=entry->uncompressedSize||output.getStatus().failed())return juce::Result::fail("The update archive is truncated or damaged.");
    }
    const auto candidate=stage.getChildFile("WUBROT.vst3");
    const auto checked=validator?validator(moduleFile(candidate),p.version):validateModule(moduleFile(candidate),p.version);if(checked.failed())return checked;
    const auto manifest=[](const juce::File& root){return root.getChildFile("Contents/Resources/moduleinfo.json");};
    if(manifest(p.target).existsAsFile()){
        auto ids=[&](const juce::File& root){juce::StringArray result;const auto data=juce::JSON::parse(manifest(root).loadFileAsString());if(const auto* classes=data["Classes"].getArray())for(const auto& entry:*classes){const auto id=entry["CID"].toString();if(id.length()==32&&id.containsOnly("0123456789abcdefABCDEF"))result.add(id.toLowerCase());}result.sort(false);return result;};
        const auto existing=ids(p.target),next=ids(candidate);if(existing.isEmpty()||existing!=next)return juce::Result::fail("The update changes the plugin identity. No files were replaced.");
    }
    const auto backup=p.work.getChildFile("rollback/WUBROT.vst3");if(backup.exists())return juce::Result::fail("An earlier rollback already exists for this update request.");
    if(backup.getParentDirectory().createDirectory().failed()||!p.target.copyDirectoryTo(backup)||hashFile(moduleFile(backup))!=p.installedSha256)return juce::Result::fail("Could not save the previous version. No files were replaced.");
    const auto move=[&](const juce::File& a,const juce::File& b){if(mover)return mover(a,b);
       #if JUCE_WINDOWS
        return MoveFileExW(a.getFullPathName().toWideCharPointer(),b.getFullPathName().toWideCharPointer(),MOVEFILE_WRITE_THROUGH)!=0;
       #else
        return std::rename(a.getFullPathName().toRawUTF8(),b.getFullPathName().toRawUTF8())==0;
       #endif
    };
    if(beforeSwap){const auto ready=beforeSwap();if(ready.failed())return ready;}
    if(!move(p.target,old)){if(afterSwap)afterSwap();return juce::Result::fail("WUBROT is still in use or its installation folder is not writable. No files were replaced.");}
    if(!move(candidate,p.target)){
        const auto restored=move(old,p.target);if(afterSwap)afterSwap();
        if(!restored)return juce::Result::fail("Installation and automatic rollback failed. The old bundle is preserved at "+old.getFullPathName()+" and "+backup.getFullPathName());
        return juce::Result::fail("Installation failed. The previous version was restored.");
    }
    if(afterSwap)afterSwap();
    // Old files are outside the scan tree in the retained rollback copy.
    old.deleteRecursively();return juce::Result::ok();
}
}
