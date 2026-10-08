#pragma once

#include <string>
#include <vector>

struct VersionInfo {
    std::wstring name;       // e.g. "1.16.100.4"
    std::wstring updateId;   // Store UpdateID (GUID)
};

namespace Versions {

    // Release versions from the MCLauncher version database, newest first.
    // Falls back to the cached copy, and always contains Globals::DEFAULT_VERSION.
    std::vector<VersionInfo> Load(std::wstring& status);

    // "1.16.100.4" -> "Minecraft_1.16.100.04_x64" (the build number is padded to 2 digits).
    std::wstring FolderName(const std::wstring& version);

    // <exe dir>\imported_versions
    std::wstring Root();

    // <exe dir>\imported_versions\Minecraft_1.16.100.04_x64
    std::wstring Dir(const std::wstring& version);

    // Marker written after a successful unpack; its presence means the folder is complete.
    std::wstring ExtractedMarker(const std::wstring& version);
    bool IsExtracted(const std::wstring& version);

    // "1.16" for "1.16.100.4" — used to group the menu.
    std::wstring Major(const std::wstring& version);

} // namespace Versions
