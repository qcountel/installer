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

    // Registers gameDir. If a Store copy is installed, its worlds (LocalState\games\com.mojang)
    // are copied to backupRoot first and restored into the new registration.
    bool Register(const std::wstring& gameDir, const std::wstring& backupRoot,
                  const std::function<void(unsigned)>& onProgress,
                  const std::function<void(const std::wstring&)>& onStatus,
                  std::wstring& status);

    // Starts the registered game.
    bool Launch();

    // Opens a folder in Explorer.
    void OpenFolder(const std::wstring& dir);

} // namespace Installer
