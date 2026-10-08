#pragma once

#include <string>

// Windows Update protocol, the same request MCLauncher uses (MCLauncher/WUProtocol.cs):
// asks Microsoft's update service for the CDN link of a Store package by its UpdateID.
// Release builds do not need a Microsoft account token.
namespace WU {

    // Resolves a direct http://tlu.dl.delivery.mp.microsoft.com/... link for the package.
    bool ResolveDownloadUrl(const std::wstring& updateId, std::wstring& url, std::wstring& status);

} // namespace WU
