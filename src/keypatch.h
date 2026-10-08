#pragma once

#include <string>

// Built-in port of KeyPatcher by ambiennt (GPLv3): https://github.com/ambiennt/KeyPatcher
// Old Minecraft builds ship Mojang's previous Xbox Live root public key, so sign-in fails.
// The patch replaces that key inside Minecraft.Windows.exe with the current one (same length).
namespace KeyPatch {

    enum class Result {
        Patched,          // old key found and replaced
        AlreadyPatched,   // new key already present
        KeyNotFound,      // neither key: build predates Xbox auth or uses another key
        Error             // could not read/write the file
    };

    // Finds Minecraft.Windows.exe in the game folder (top level first, then subfolders).
    std::wstring FindExecutable(const std::wstring& gameDir);

    Result Patch(const std::wstring& exePath, std::wstring& status);

} // namespace KeyPatch
