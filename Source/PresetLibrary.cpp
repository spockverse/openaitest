#include "PresetLibrary.h"
#include <charconv>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace
{
constexpr juce::int64 limit = 2 * 1024 * 1024;
bool number(const juce::var &value, double &d)
{
    const auto text = value.toString();
    if (text.isEmpty() || !text.containsOnly("0123456789.eE+-"))
        return false;
    const auto *first = text.toRawUTF8();
    const auto *last = first + text.getNumBytesAsUTF8();
    if (*first == '+')
        ++first;
    const auto parsed = std::from_chars(first, last, d, std::chars_format::general);
    return parsed.ec == std::errc() && parsed.ptr == last && std::isfinite(d);
}
} // namespace
PresetLibrary::PresetLibrary(juce::File documents)
{
    if (documents == juce::File())
        documents = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    if (documents != juce::File())
        root = documents.getChildFile("Wubware Audio/WUBROT/Presets");
}
juce::Result PresetLibrary::ensureDirectory() const
{
    if (root == juce::File())
        return juce::Result::fail("Windows Documents could not be resolved. No alternate folder was used.");
    auto r = root.createDirectory();
    if (r.failed())
        return juce::Result::fail("Cannot create the preset folder in Documents: " + r.getErrorMessage());
    if (!root.hasWriteAccess())
        return juce::Result::fail("The Documents preset folder is read-only or unavailable.");
    return juce::Result::ok();
}
juce::Result PresetLibrary::validName(const juce::String &n)
{
    if (n.isEmpty() || n.length() > 80 || n != n.trim() || n.endsWithChar('.') ||
        n.containsAnyOf("<>:\"/\\|?*") || n == "." || n == "..")
        return juce::Result::fail("Use 1–80 characters, with no leading/trailing spaces, trailing dot, or "
                                  "Windows filename characters <>:\"/\\|?*.");
    for (auto c : n)
        if (c < 32 || c == 127)
            return juce::Result::fail("Preset names cannot contain control characters.");
    const auto first = n.upToFirstOccurrenceOf(".", false, false).toUpperCase();
    if (juce::StringArray{"CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
                          "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
                          "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"}
            .contains(first))
        return juce::Result::fail("That name is reserved by Windows. Choose another name.");
    return juce::Result::ok();
}
juce::File PresetLibrary::fileFor(const juce::String &n) const
{
    if (validName(n).failed() || root == juce::File())
        return {};
    // Explicitly case-insensitive even on the Linux regression-test machine.
    for (const auto &f : root.findChildFiles(juce::File::findFiles, false, "*.wubpreset"))
        if (f.getFileNameWithoutExtension().equalsIgnoreCase(n))
            return f;
    return root.getChildFile(n + ".wubpreset");
}
juce::File PresetLibrary::resolve(const juce::String &f) const
{
    if (!f.endsWithIgnoreCase(".wubpreset") || validName(f.dropLastCharacters(10)).failed())
        return {};
    return fileFor(f.dropLastCharacters(10));
}
juce::Array<juce::var> PresetLibrary::list(juce::String &error) const
{
    juce::Array<juce::var> a;
    auto r = ensureDirectory();
    if (r.failed())
    {
        error = r.getErrorMessage();
        if (!root.isDirectory() || !root.hasReadAccess())
            return a;
    }
    auto files = root.findChildFiles(juce::File::findFiles, false, "*.wubpreset");
    files.sort();
    for (const auto &f : files)
    {
        auto *o = new juce::DynamicObject();
        o->setProperty("id", f.getFileName());
        o->setProperty("name", f.getFileNameWithoutExtension());
        a.add(o);
    }
    return a;
}
juce::Result PresetLibrary::validate(const juce::ValueTree &s)
{
    if (!s.hasType("WubrotState") || s.hasProperty("sourceWav"))
        return juce::Result::fail(
            "Invalid preset state or embedded audio. Presets must contain analysis data only.");
    if(s.hasProperty("xyMapping"))
    {
        double version=0;
        if(!number(s["xyMapping"],version)||(version!=1&&version!=2&&version!=3&&version!=4&&version!=5&&version!=6&&version!=7&&version!=8&&version!=9))
            return juce::Result::fail("Unsupported XY mapping version. Update WUBROT to load this preset.");
    }
    struct Bound
    {
        const char *id;
        double lo, hi;
        bool discrete;
    };
    constexpr Bound bounds[]{
        {"x", -1, 1, false},          {"y", -1, 1, false},        {"mutate", 0, 100, false},
        {"drive", 0, 36, false},      {"mode", 0, 1, true},       {"mix", 0, 100, false},
        {"output", -24, 6, false},    {"protect", 0, 1, true},    {"enabled", 0, 1, true},
        {"regionStart", 0, 1, false}, {"regionEnd", 0, 1, false}, {"dnaSpeed", 0, 4, true}};
    if (s.getNumChildren() != 12)
        return juce::Result::fail("The preset must contain all 12 WUBROT parameters.");
    for (const auto &b : bounds)
    {
        int count = 0;
        for (auto c : s)
            if (c.hasType("PARAM") && c["id"].toString() == b.id)
            {
                const auto v = c["value"];
                double d = 0;
                if (!number(v, d) || d < b.lo || d > b.hi || (b.discrete && d != std::round(d)))
                    return juce::Result::fail("Invalid parameter value: " + juce::String(b.id));
                ++count;
            }
        if (count != 1)
            return juce::Result::fail("Missing or duplicate parameter: " + juce::String(b.id));
    }
    if (static_cast<float>(s.getChildWithProperty("id", "regionStart")["value"]) + .005f >
        static_cast<float>(s.getChildWithProperty("id", "regionEnd")["value"]))
        return juce::Result::fail("DNA START must precede END.");
    for (const char *id : {"analysisStart", "analysisEnd"})
    {
        double d = 0;
        const bool validNumber = number(s[id], d);
        const auto parameter = juce::String(id) == "analysisStart" ? "regionStart" : "regionEnd";
        if (!validNumber ||
            std::abs(d - static_cast<double>(s.getChildWithProperty("id", parameter)["value"])) > 1e-5)
            return juce::Result::fail("The active DNA region does not match its START/END parameters.");
    }
    DnaProfile active;
    if (!s.hasProperty("presetProfile") ||
        !WubrotPresetData::readProfile(juce::JSON::parse(s["presetProfile"].toString()), active))
        return juce::Result::fail("Missing or corrupt active DNA profile.");
    if(int(s.getProperty("xyMapping",1))>=3&&active.valid&&active.spectralCount==0)
        return juce::Result::fail("Missing version-2 temporal DNA descriptors for this sound.");
    std::shared_ptr<const WubrotPresetData::Archive> archive;
    if (s.hasProperty("dnaArchive"))
    {
        archive = WubrotPresetData::Archive::fromVar(juce::JSON::parse(s["dnaArchive"].toString()));
        if (!archive)
            return juce::Result::fail("Corrupt DNA timeline or spectral archive.");
    }
    if (active.valid && !archive)
        return juce::Result::fail("This preset is missing the full DNA analysis needed for region editing.");
    return juce::Result::ok();
}
juce::Result PresetLibrary::read(const juce::File &f, juce::ValueTree &state, juce::String &name)
{
    if (!f.existsAsFile() || !f.hasReadAccess())
        return juce::Result::fail("The preset file is missing or inaccessible.");
    if (f.getSize() < 1 || f.getSize() > limit)
        return juce::Result::fail("Preset size is invalid (maximum 2 MB).");
    const auto text = f.loadFileAsString();
    if (text.containsIgnoreCase("<!DOCTYPE") || text.containsIgnoreCase("<!ENTITY"))
        return juce::Result::fail("XML document types and entities are not supported.");
    auto xml = juce::parseXML(text);
    if (!xml || !xml->hasTagName("WubrotPreset") || xml->getStringAttribute("plugin") != "WUBROT")
        return juce::Result::fail("This is not a compatible WUBROT preset.");
    if (xml->getStringAttribute("formatVersion") != "1" && xml->getStringAttribute("formatVersion") != "2" && xml->getStringAttribute("formatVersion") != "3" && xml->getStringAttribute("formatVersion") != "4" && xml->getStringAttribute("formatVersion") != "5" && xml->getStringAttribute("formatVersion") != "6" && xml->getStringAttribute("formatVersion") != "7" && xml->getStringAttribute("formatVersion") != "8" && xml->getStringAttribute("formatVersion") != "9")
        return juce::Result::fail("Unsupported preset version. Update WUBROT to open this file.");
    name = xml->getStringAttribute("name");
    auto r = validName(name);
    if (r.failed())
        return r;
    if (xml->getNumChildElements() != 1 || !xml->getFirstChildElement()->hasTagName("WubrotState"))
        return juce::Result::fail("The preset state is missing or malformed.");
    auto candidate = juce::ValueTree::fromXml(*xml->getFirstChildElement());
    if(xml->getIntAttribute("formatVersion")!=static_cast<int>(candidate.getProperty("xyMapping",1)))
        return juce::Result::fail("Preset container and XY mapping version do not agree.");
    r = validate(candidate);
    if (r.failed())
        return r;
    state = candidate;
    return juce::Result::ok();
}
juce::Result PresetLibrary::write(const juce::File &f, const juce::ValueTree &state, const juce::String &name,
                                  bool overwrite)
{
    auto r = validName(name);
    if (r.failed())
        return r;
    r = validate(state);
    if (r.failed())
        return r;
    juce::InterProcessLock fileLock(
        "WUBROT-preset-" + juce::String::toHexString(f.getFullPathName().toLowerCase().hashCode64()));
    if (!fileLock.enter(5000))
        return juce::Result::fail("Another WUBROT instance is writing this preset. Try again.");
    struct Unlock
    {
        juce::InterProcessLock &lock;
        ~Unlock() { lock.exit(); }
    } unlock{fileLock};
    if (f.isSymbolicLink())
        return juce::Result::fail("Preset destinations cannot be symbolic links.");
    if (f == juce::File() || !f.getParentDirectory().isDirectory() ||
        !f.getParentDirectory().hasWriteAccess())
        return juce::Result::fail("The destination folder is unavailable or read-only.");
    if (f.existsAsFile() && !f.hasWriteAccess())
        return juce::Result::fail("The destination preset is read-only. Existing preset preserved.");
    if (f.exists() && !overwrite)
        return juce::Result::fail(
            "A preset with that name already exists. Choose Overwrite or a different name.");
    juce::XmlElement xml("WubrotPreset");
    xml.setAttribute("formatVersion", static_cast<int>(state.getProperty("xyMapping",1)));
    xml.setAttribute("plugin", "WUBROT");
    xml.setAttribute("name", name);
    xml.addChildElement(state.createXml().release());
    const auto text = xml.toString(juce::XmlElement::TextFormat().singleLine());
    if (text.getNumBytesAsUTF8() > limit)
        return juce::Result::fail("Preset analysis exceeds the 2 MB format limit.");
    juce::TemporaryFile temporary(f);
    {
        auto stream = temporary.getFile().createOutputStream();
        if (!stream || !stream->writeText(text, false, false, nullptr))
            return juce::Result::fail(
                "Could not write the temporary preset file. Existing preset preserved.");
        stream->flush();
        if (stream->getStatus().failed())
            return juce::Result::fail("Could not flush preset data. Existing preset preserved.");
    }
    juce::ValueTree check;
    juce::String checkedName;
    r = read(temporary.getFile(), check, checkedName);
    if (r.failed() || checkedName != name || temporary.getFile().loadFileAsString() != text)
        return juce::Result::fail("Temporary preset validation failed. Existing preset preserved.");
    if (!overwrite && f.exists())
        return juce::Result::fail("A preset appeared at that location while saving. Try a different name.");
    bool committed = false;
    if (overwrite)
        committed = temporary.overwriteTargetFileWithTemporary();
    else
    {
#if JUCE_WINDOWS
        committed = MoveFileExW(temporary.getFile().getFullPathName().toWideCharPointer(),
                                f.getFullPathName().toWideCharPointer(), MOVEFILE_WRITE_THROUGH) != 0;
#else
        committed =
            ::link(temporary.getFile().getFullPathName().toRawUTF8(), f.getFullPathName().toRawUTF8()) == 0;
#endif
    }
    if (!committed)
        return juce::Result::fail("Could not commit the preset file. Existing preset preserved; check "
                                  "permissions or OneDrive/file locks.");
    return juce::Result::ok();
}
juce::Result PresetLibrary::save(const juce::ValueTree &s, const juce::String &name, bool overwrite) const
{
    auto r = ensureDirectory();
    return r.failed() ? r : write(fileFor(name), s, name, overwrite);
}
juce::Result PresetLibrary::rename(const juce::String &filename, const juce::String &name,
                                   bool overwrite) const
{
    const auto original = resolve(filename);
    if (original == juce::File())
        return juce::Result::fail("Invalid user preset filename.");
    auto r = validName(name);
    if (r.failed())
        return r;
    juce::ValueTree s;
    juce::String old;
    r = read(original, s, old);
    if (r.failed())
        return r;
    if (name.equalsIgnoreCase(original.getFileNameWithoutExtension()) &&
        name != original.getFileNameWithoutExtension())
    {
        r = write(original, s, name, true);
        if (r.failed())
            return r;
        const auto desired = root.getChildFile(name + ".wubpreset");
#if JUCE_WINDOWS
        const bool moved =
            MoveFileExW(original.getFullPathName().toWideCharPointer(),
                        desired.getFullPathName().toWideCharPointer(), MOVEFILE_WRITE_THROUGH) != 0;
#else
        const bool moved =
            ::rename(original.getFullPathName().toRawUTF8(), desired.getFullPathName().toRawUTF8()) == 0;
#endif
        return moved ? juce::Result::ok()
                     : juce::Result::fail("The preset sound is preserved, but its filename capitalization "
                                          "could not be changed.");
    }
    auto destination = fileFor(name);
    if (destination == original && name == original.getFileNameWithoutExtension())
        return juce::Result::ok();
    // A case-only rename is safe through the validated replace path; otherwise never destroy source before
    // destination is committed.
    r = write(destination, s, name, overwrite || destination == original);
    if (r.failed())
        return r;
    if (destination != original && !original.deleteFile())
        return juce::Result::fail("Renamed copy saved, but the original could not be removed.");
    return juce::Result::ok();
}
juce::Result PresetLibrary::remove(const juce::String &filename) const
{
    const auto f = resolve(filename);
    if (f == juce::File() || !f.existsAsFile())
        return juce::Result::fail("User preset not found.");
    return f.deleteFile()
               ? juce::Result::ok()
               : juce::Result::fail("Cannot delete this user preset. Check permissions or file locks.");
}
