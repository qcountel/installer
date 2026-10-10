// Compiled without the precompiled header.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

#include "versions.h"
#include "config.h"
#include "json.h"
#include "net.h"

namespace {

std::vector<int> Parts(const std::wstring& v) {
    std::vector<int> out;
    std::wstringstream ss(v);
    std::wstring item;
    while (std::getline(ss, item, L'.')) out.push_back(_wtoi(item.c_str()));
    return out;
}

bool Newer(const VersionInfo& a, const VersionInfo& b) {
    return Parts(a.name) > Parts(b.name);
}

std::wstring CacheFile() {
    return Versions::Root() + L"\\versions_cache.json";
}

// [[name, updateId, type], ...] where type 0 = release, 1 = beta, 2 = preview.
bool Parse(const std::string& body, std::vector<VersionInfo>& out) {
    try {
        Json::JParser p(body);
        Json::JValue root = p.value();
        const Json::JArray* arr = root.arr();
        if (!arr) return false;
        out.clear();
        for (const Json::JValue& e : *arr) {
            const Json::JArray* row = e.arr();
            if (!row || row->size() < 3) continue;
            if ((int)(*row)[2].num() != 0) continue;   // releases only
            VersionInfo v{ Net::Widen((*row)[0].str()), Net::Widen((*row)[1].str()), PackageKind::UWP };
            if (!v.name.empty() && v.updateId.size() == 36) out.push_back(v);
        }
        return !out.empty();
    } catch (...) {
        return false;
    }
}

} // namespace

namespace Versions {

const std::vector<VersionInfo>& Available() {
    static const std::vector<VersionInfo> list = {
        { Globals::DEFAULT_VERSION, Globals::DEFAULT_UPDATE_ID, PackageKind::UWP },
        { Globals::NEW_VERSION, L"", PackageKind::GDK },
    };
    return list;
}

VersionInfo Find(const std::wstring& name) {
    for (const VersionInfo& v : Available())
        if (v.name == name) return v;
    return Available().front();
}

std::wstring MsixvcPath(const std::wstring& version) {
    return Root() + L"\\Minecraft_" + version + L"_x64.msixvc";
}

std::wstring Root() {
    return Globals::DATA_DIR + L"\\" + Globals::VERSIONS_DIR_NAME;
}

std::wstring FolderName(const std::wstring& version, const std::wstring& suffix) {
    std::wstring v = version;
    size_t dot = v.find_last_of(L'.');
    if (dot != std::wstring::npos && v.size() - dot - 1 == 1) v.insert(dot + 1, L"0");
    return L"Minecraft_" + v + L"_x64" + suffix;
}

std::wstring Dir(const std::wstring& version, const std::wstring& suffix) {
    return Root() + L"\\" + FolderName(version, suffix);
}

std::wstring ExtractedMarker(const std::wstring& version, const std::wstring& suffix) {
    return Dir(version, suffix) + L"\\.installer_extracted";
}

bool IsExtracted(const std::wstring& version, const std::wstring& suffix) {
    return GetFileAttributesW(ExtractedMarker(version, suffix).c_str()) != INVALID_FILE_ATTRIBUTES &&
           GetFileAttributesW((Dir(version, suffix) + L"\\AppxManifest.xml").c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring Major(const std::wstring& version) {
    size_t first = version.find(L'.');
    if (first == std::wstring::npos) return version;
    size_t second = version.find(L'.', first + 1);
    return version.substr(0, second);
}

std::vector<VersionInfo> Load(std::wstring& status) {
    std::vector<VersionInfo> list;
    std::string body;
    std::wstring err;

    if (Net::Get(Globals::VERSIONS_URL, body, err, 15000) && Parse(body, list)) {
        CreateDirectoryW(Root().c_str(), nullptr);
        std::ofstream(CacheFile(), std::ios::binary | std::ios::trunc) << body;
        status = L"Список версий обновлён";
    } else {
        std::ifstream f(CacheFile(), std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        if (f && Parse(ss.str(), list)) status = L"Нет связи со списком версий — показан сохранённый";
        else { list.clear(); status = L"Список версий недоступен — доступна только " + std::wstring(Globals::DEFAULT_VERSION); }
    }

    // The default version must always be there with its known UpdateID.
    list.erase(std::remove_if(list.begin(), list.end(),
        [](const VersionInfo& v) { return v.name == Globals::DEFAULT_VERSION; }), list.end());
    list.push_back({ Globals::DEFAULT_VERSION, Globals::DEFAULT_UPDATE_ID, PackageKind::UWP });

    // Drop duplicates, newest first.
    std::stable_sort(list.begin(), list.end(), Newer);
    std::set<std::wstring> seen;
    list.erase(std::remove_if(list.begin(), list.end(),
        [&](const VersionInfo& v) { return !seen.insert(v.name).second; }), list.end());
    return list;
}

} // namespace Versions
