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
