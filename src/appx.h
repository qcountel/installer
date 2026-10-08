#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <cwctype>

namespace Appx {

    // Unpacks an .appx (a ZIP/OPC package) into dir, like MCLauncher does:
    // every file is extracted except AppxSignature.p7x, so the folder can be registered
    // in Developer Mode and its files (Minecraft.Windows.exe) can be modified.
    // onProgress(doneBytes, totalBytes) counts uncompressed bytes.
    bool Extract(const std::wstring& appxPath, const std::wstring& dir,
                 const std::function<void(uint64_t, uint64_t)>& onProgress,
                 const std::atomic<bool>* cancel, std::wstring& status);

    // Extracts the first entry whose name ends with `suffix` (case-insensitive, e.g. L".appx")
    // from a plain .zip to outPath. Used for the Google Drive build (zip with .appx + .cer).
    // foundName receives the entry name.
    bool ExtractEntry(const std::wstring& zipPath, const std::wstring& suffix, const std::wstring& outPath,
                      const std::function<void(uint64_t, uint64_t)>& onProgress,
                      const std::atomic<bool>* cancel, std::wstring& foundName, std::wstring& status);

    // Reads a small entry (e.g. "AppxManifest.xml") of a zip/appx into memory.
    bool ReadEntry(const std::wstring& zipPath, const char* name, std::string& out, std::wstring& status);

} // namespace Appx
