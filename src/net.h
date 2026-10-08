#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace Net {

    // GET a small resource (JSON, text) into memory. timeoutMs applies to connect and receive.
    bool Get(const std::wstring& url, std::string& body, std::wstring& status, unsigned timeoutMs = 60000);

    // POST body with the given Content-Type and read the whole response.
    bool Post(const std::wstring& url, const std::string& body, const wchar_t* contentType,
              std::string& response, std::wstring& status);

    // Streams url into path (via "<path>.part"). onBytes(done, total) — total is 0 if the server
    // did not send Content-Length. Setting cancel to true aborts the download.
    bool DownloadFile(const std::wstring& url, const std::wstring& path,
                      const std::function<void(uint64_t, uint64_t)>& onBytes,
                      const std::atomic<bool>* cancel, std::wstring& status);

    // SHA-256 of a file as lowercase hex, empty on error.
    std::string Sha256File(const std::wstring& path);

    std::wstring Widen(const std::string& s);
    std::string  Narrow(const std::wstring& s);

} // namespace Net
