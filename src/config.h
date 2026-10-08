#pragma once

#include <cstdint>
#include <string>

namespace Globals {

    // Folder that contains the installer .exe.
    inline std::wstring EXE_DIR{};

    // Hidden + system folder with everything the installer creates (config, versions, backups),
    // so only the .exe stays visible: %LOCALAPPDATA%\MinecraftInstaller.
    inline std::wstring DATA_DIR{};
    inline const wchar_t* DATA_DIR_NAME = L"MinecraftInstaller";

    // ---- Default version: always preselected on start, available even when the online list is down ----
    inline const wchar_t* DEFAULT_VERSION   = L"1.16.100.4";
    inline const wchar_t* DEFAULT_UPDATE_ID = L"d8107807-40f7-4552-a78c-f410fe1ae1df";
    // Exact package Microsoft serves for the default version (checked after download).
    inline const uint64_t DEFAULT_APPX_SIZE   = 308772848ull;
    inline const char*    DEFAULT_APPX_SHA256 = "e154f79aec8b5b5bedb195171ac4a27902e6cf935009cc8cb8be48fc2ed1fd6c";

    // Version database used by MCLauncher: JSON array of [name, updateId, type], type 0 = release.
    inline std::wstring VERSIONS_URL{ L"https://mrarm.io/r/w10-vdb" };

    // Delete the downloaded .appx after it has been unpacked.
    inline bool DELETE_APPX = true;

    // Patch Minecraft.Windows.exe with the new Xbox Live public key (KeyPatcher).
    inline bool APPLY_KEYPATCH = true;

    // Folder inside DATA_DIR where versions are unpacked, like MCLauncher's imported_versions.
    inline const wchar_t* VERSIONS_DIR_NAME = L"imported_versions";

    // Minecraft for Windows 10 (UWP) package identity.
    inline const wchar_t* PACKAGE_FAMILY = L"Microsoft.MinecraftUWP_8wekyb3d8bbwe";
    inline const wchar_t* APP_ID         = L"Microsoft.MinecraftUWP_8wekyb3d8bbwe!App";

} // namespace Globals

class Config {
    std::wstring path;
    void apply(const std::wstring& key, const std::wstring& val);
public:
    Config();
    bool load();
    bool save() const;
};
