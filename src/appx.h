#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace Appx {

    // Unpacks an .appx (a ZIP/OPC package) into dir, like MCLauncher does:
    // every file is extracted except AppxSignature.p7x, so the folder can be registered
    // in Developer Mode and its files (Minecraft.Windows.exe) can be modified.
    // onProgress(doneBytes, totalBytes) counts uncompressed bytes.
    bool Extract(const std::wstring& appxPath, const std::wstring& dir,
                 const std::function<void(uint64_t, uint64_t)>& onProgress,
                 const std::atomic<bool>* cancel, std::wstring& status);

} // namespace Appx
