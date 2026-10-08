#pragma once

#include <functional>
#include <string>

// Registration of an unpacked Minecraft folder, the way MCLauncher does it
// (MainWindow.ReRegisterPackage): the existing Minecraft package is removed and
// <game dir>\AppxManifest.xml is registered with DeploymentOptions::DevelopmentMode.
namespace Installer {

    // Settings -> For developers -> Developer Mode (required to register unsigned folders).
    bool IsDeveloperModeEnabled();
    void OpenDeveloperSettings();

    // Minecraft (UWP) package currently installed for this user.
    struct PackageState {
        bool found = false;
        bool devMode = false;       // registered unpacked folder (Developer Mode)
        std::wstring location;
        std::wstring version;       // e.g. "1.16.10004.0"
    };
    PackageState Current();

    // Folder of the Minecraft (UWP) package registered for the current user, empty if none.
    std::wstring RegisteredLocation();

    bool IsGameRunning();

    // Installs the framework packages listed in <gameDir>\AppxManifest.xml (Microsoft.VCLibs.140.00,
    // Microsoft.Services.Store.Engagement) that are missing on this PC. They are downloaded from
    // Windows Update into depsDir and checked by size + SHA-256. Without them registration fails
    // with 0x80073CF3.
    bool EnsureDependencies(const std::wstring& gameDir, const std::wstring& depsDir,
                            const std::function<void(const std::wstring&)>& onStatus,
                            std::wstring& status);

    // Same check for a manifest that is already in memory (e.g. read from a signed .appx).
    bool EnsureDependenciesFor(const std::string& manifest, const std::wstring& depsDir,
                               const std::function<void(const std::wstring&)>& onStatus,
                               std::wstring& status);

    // Registers gameDir. If a Store copy is installed, its worlds (LocalState\games\com.mojang)
    // are copied to backupRoot first and restored into the new registration.
    bool Register(const std::wstring& gameDir, const std::wstring& backupRoot,
                  const std::function<void(unsigned)>& onProgress,
                  const std::function<void(const std::wstring&)>& onStatus,
                  std::wstring& status);

    // Installs a signed .appx (the Google Drive build): its certificate goes to
    // Local Machine\Trusted People, the current Minecraft is removed (worlds backed up and restored)
    // and the package is added normally — Developer Mode is not needed.
    bool InstallSigned(const std::wstring& appxPath, const std::wstring& cerPath, const std::wstring& backupRoot,
                       const std::function<void(unsigned)>& onProgress,
                       const std::function<void(const std::wstring&)>& onStatus,
                       std::wstring& status);

    // Removes the Minecraft package installed by this program: a Developer Mode registration from a
    // folder under versionsRoot, or the signed Drive build (signedVersion). A Store copy is never touched.
    // Worlds are copied to backupRoot first. removed = a package was actually removed.
    bool Uninstall(const std::wstring& versionsRoot, const std::wstring& signedVersion,
                   const std::wstring& backupRoot,
                   const std::function<void(const std::wstring&)>& onStatus,
                   bool& removed, std::wstring& status);

    // True if the installed Minecraft package is one this program installed (see Uninstall).
    bool IsOurs(const PackageState& pkg, const std::wstring& versionsRoot, const std::wstring& signedVersion);

    // Starts the registered game.
    bool Launch();

    // Opens a folder in Explorer.
    void OpenFolder(const std::wstring& dir);

} // namespace Installer
