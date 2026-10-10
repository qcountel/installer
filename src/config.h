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

    // ---- New version: official GDK build (.msixvc) straight from Microsoft's Xbox CDN ----
    // No Google Drive build, no unpacking and no KeyPatcher: the signed package is installed and
    // registered as is (like the Store does). Links: MinecraftBedrockArchiver/GdkLinks (used by MCLauncher).
    inline const wchar_t* NEW_VERSION         = L"26.52.3";
    inline const wchar_t* NEW_PACKAGE_VERSION = L"1.26.5203.0";   // version inside the package
    inline const wchar_t* NEW_MSIXVC_URLS[] = {
        L"http://assets1.xboxlive.com/Z/80c6fe1f-3d12-47fe-ace0-1406ce1bc3c6/7792d9ce-355a-493c-afbd-768f4a77c3b0/1.26.5203.0.8faf62df-3520-469d-9462-64acfbabde93/Microsoft.MinecraftUWP_1.26.5203.0_x64__8wekyb3d8bbwe.msixvc",
        L"http://assets2.xboxlive.com/Z/80c6fe1f-3d12-47fe-ace0-1406ce1bc3c6/7792d9ce-355a-493c-afbd-768f4a77c3b0/1.26.5203.0.8faf62df-3520-469d-9462-64acfbabde93/Microsoft.MinecraftUWP_1.26.5203.0_x64__8wekyb3d8bbwe.msixvc",
    };
    inline const uint64_t NEW_MSIXVC_SIZE = 2069975040ull;

    // GDK games need Gaming Services (Microsoft Store: 9MWPM2CQNLHN).
    inline const wchar_t* GAMING_SERVICES_FAMILY = L"Microsoft.GamingServices_8wekyb3d8bbwe";

    // Version chosen on the install page (saved in config.txt).
    inline std::wstring SELECTED_VERSION{ DEFAULT_VERSION };

    // Version database used by MCLauncher: JSON array of [name, updateId, type], type 0 = release.
    inline std::wstring VERSIONS_URL{ L"https://mrarm.io/r/w10-vdb" };

    // Delete the downloaded package (.appx / .msixvc) after it has been unpacked / installed.
    inline bool DELETE_APPX = true;

    // Patch Minecraft.Windows.exe with the new Xbox Live public key (KeyPatcher). 1.16.100.4 only.
    inline bool APPLY_KEYPATCH = true;

    // ---- Custom 1.16.100.4 build from Google Drive (unofficial build, zip: .appx + .cer) ----
    // Settings -> "Скачивание с Google Дисков". Default ON; applies to 1.16.100.4 only.
    inline bool GDRIVE_ENABLED = true;
    inline std::wstring GDRIVE_URL{
        L"https://drive.usercontent.google.com/download?id=1cSxfij--4QNUkB9MbbLdoIpFz2XpLNXD&export=download&confirm=t" };
    // How the Drive build is installed:
    //   Dev  — unpack + register in Developer Mode (no certificate, KeyPatcher works)  [default]
    //   Cert — trust the bundled certificate and install the signed .appx (no Developer Mode,
    //          but Minecraft.Windows.exe cannot be patched, so Xbox Live sign-in will not work)
    enum class GDriveMode { Dev, Cert };
    inline GDriveMode GDRIVE_MODE = GDriveMode::Dev;

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
