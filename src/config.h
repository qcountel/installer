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

    // ---- Custom 1.16.100.4 build from Google Drive (unofficial build, zip: .appx + .cer) ----
    // Settings -> "Скачивание с Google Дисков". Default ON; applies to 1.16.100.4 only.
    inline bool GDRIVE_ENABLED = true;
    inline std::wstring GDRIVE_URL{
        L"https://drive.usercontent.google.com/download?id=1pkiVyhJfzhSi_fsiyjLJlUdCKttR685g&export=download&confirm=t" };
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
