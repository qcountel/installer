#pragma once

#include <string>
#include <vector>

// UWP = .appx from Windows Update, unpacked and registered in Developer Mode (1.16.100.4).
// GDK = signed .msixvc from the Xbox CDN, installed as is (26.52.3).
enum class PackageKind { UWP, GDK };

struct VersionInfo {
    std::wstring name;       // e.g. "1.16.100.4"
    std::wstring updateId;   // Store UpdateID (GUID), UWP only
    PackageKind kind = PackageKind::UWP;
};

namespace Versions {

    // The versions offered by the installer: 1.16.100.4 and 26.52.3.
    const std::vector<VersionInfo>& Available();
    // Available() entry with this name, or the default version.
    VersionInfo Find(const std::wstring& name);

    // <DATA_DIR>\imported_versions\Minecraft_26.52.3_x64.msixvc
    std::wstring MsixvcPath(const std::wstring& version);

    // Release versions from the MCLauncher version database, newest first.
    // Falls back to the cached copy, and always contains Globals::DEFAULT_VERSION.
    std::vector<VersionInfo> Load(std::wstring& status);

    // "1.16.100.4" -> "Minecraft_1.16.100.04_x64" (the build number is padded to 2 digits).
    std::wstring FolderName(const std::wstring& version, const std::wstring& suffix = L"");

    // <DATA_DIR>\imported_versions
    std::wstring Root();

    // <DATA_DIR>\imported_versions\Minecraft_1.16.100.04_x64
    std::wstring Dir(const std::wstring& version, const std::wstring& suffix = L"");

    // Marker written after a successful unpack; its presence means the folder is complete.
    std::wstring ExtractedMarker(const std::wstring& version, const std::wstring& suffix = L"");
    bool IsExtracted(const std::wstring& version, const std::wstring& suffix = L"");

    // Folder suffixes of the Google Drive build (kept apart from the official one).
    inline const wchar_t* GDRIVE_SUFFIX = L"_gdrive";          // unpacked, Developer Mode
    inline const wchar_t* GDRIVE_SIGNED_SUFFIX = L"_gdrive_signed";   // signed .appx + .cer

    // "1.16" for "1.16.100.4" — used to group the menu.
    std::wstring Major(const std::wstring& version);

} // namespace Versions
