// HTTP helpers on top of WinINet (follows redirects, works with http:// and https://).
// Compiled without the precompiled header.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wininet.h>
#include <bcrypt.h>

#include <iterator>
#include <vector>

#include "net.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "bcrypt.lib")

namespace {

struct InetHandle {
    HINTERNET h = nullptr;
    explicit InetHandle(HINTERNET x) : h(x) {}
    ~InetHandle() { if (h) InternetCloseHandle(h); }
    InetHandle(const InetHandle&) = delete;
    InetHandle& operator=(const InetHandle&) = delete;
    operator HINTERNET() const { return h; }
};

HINTERNET OpenSession(unsigned timeoutMs) {
    HINTERNET h = InternetOpenW(L"Minecraft-Installer/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (h) {
        DWORD timeout = timeoutMs;
        InternetSetOptionW(h, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionW(h, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionW(h, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    }
    return h;
}

bool IsHttps(const std::wstring& url) {
    return _wcsnicmp(url.c_str(), L"https://", 8) == 0;
}

DWORD RequestFlags(const std::wstring& url) {
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI |
                  INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_COOKIES |
                  // redirects between http and https are allowed (Microsoft's CDN uses http)
                  INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS;
    if (IsHttps(url)) flags |= INTERNET_FLAG_SECURE;
    return flags;
}

std::wstring ErrorText(DWORD code) {
    switch (code) {
    case ERROR_INTERNET_TIMEOUT:            return L"сервер не отвечает (тайм-аут)";
    case ERROR_INTERNET_NAME_NOT_RESOLVED:  return L"нет интернета или сервер недоступен";
    case ERROR_INTERNET_CANNOT_CONNECT:     return L"не удалось подключиться к серверу";
    case ERROR_INTERNET_CONNECTION_RESET:
    case ERROR_INTERNET_CONNECTION_ABORTED: return L"соединение оборвалось";
    default:                                return L"ошибка сети, код " + std::to_wstring(code);
    }
}

// Returns false (and sets status) for HTTP status codes >= 400.
bool CheckStatus(HINTERNET req, std::wstring& status) {
    DWORD code = 0, len = sizeof(code);
    if (!HttpQueryInfoW(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &len, nullptr))
        return true;
    if (code < 400) return true;
    if (code == 404)      status = L"файл не найден на сервере (404)";
    else if (code == 403) status = L"сервер отказал в доступе (403)";
    else                  status = L"ошибка сервера HTTP " + std::to_wstring(code);
    return false;
}

uint64_t ContentLength(HINTERNET req) {
    wchar_t buf[32]{};
    DWORD len = sizeof(buf);
    if (!HttpQueryInfoW(req, HTTP_QUERY_CONTENT_LENGTH, buf, &len, nullptr)) return 0;
    return _wcstoui64(buf, nullptr, 10);
}

bool ReadAll(HINTERNET req, std::string& out, std::wstring& status) {
    out.clear();
    char buf[16384];
    DWORD read = 0;
    for (;;) {
        if (!InternetReadFile(req, buf, sizeof(buf), &read)) {
            status = ErrorText(GetLastError());
            return false;
        }
        if (read == 0) return true;
        out.append(buf, read);
    }
}

} // namespace

namespace Net {

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string Narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

bool Get(const std::wstring& url, std::string& body, std::wstring& status, unsigned timeoutMs) {
    InetHandle session(OpenSession(timeoutMs));
    if (!session) { status = L"не удалось инициализировать интернет"; return false; }
    InetHandle req(InternetOpenUrlW(session, url.c_str(), nullptr, 0, RequestFlags(url), 0));
    if (!req) { status = ErrorText(GetLastError()); return false; }
    if (!CheckStatus(req, status)) return false;
    return ReadAll(req, body, status);
}

bool Post(const std::wstring& url, const std::string& body, const wchar_t* contentType,
          std::string& response, std::wstring& status) {
    URL_COMPONENTSW uc{ sizeof(uc) };
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host;  uc.dwHostNameLength = (DWORD)std::size(host);
    uc.lpszUrlPath = path;   uc.dwUrlPathLength = (DWORD)std::size(path);
    if (!InternetCrackUrlW(url.c_str(), 0, 0, &uc)) { status = L"неверный адрес " + url; return false; }

    InetHandle session(OpenSession(60000));
    if (!session) { status = L"не удалось инициализировать интернет"; return false; }
    InetHandle conn(InternetConnectW(session, host, uc.nPort, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0));
    if (!conn) { status = ErrorText(GetLastError()); return false; }
    InetHandle req(HttpOpenRequestW(conn, L"POST", path, nullptr, nullptr, nullptr, RequestFlags(url), 0));
    if (!req) { status = ErrorText(GetLastError()); return false; }

    std::wstring headers = std::wstring(L"Content-Type: ") + contentType + L"\r\n";
    if (!HttpSendRequestW(req, headers.c_str(), (DWORD)headers.size(), (LPVOID)body.data(), (DWORD)body.size())) {
        status = ErrorText(GetLastError());
        return false;
    }
    if (!CheckStatus(req, status)) return false;
    return ReadAll(req, response, status);
}

bool DownloadFile(const std::wstring& url, const std::wstring& path,
                  const std::function<void(uint64_t, uint64_t)>& onBytes,
                  const std::atomic<bool>* cancel, std::wstring& status) {
    InetHandle session(OpenSession(60000));
    if (!session) { status = L"не удалось инициализировать интернет"; return false; }

    std::wstring part = path + L".part";
    HANDLE file = CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { status = L"не удалось создать файл для загрузки"; return false; }

    // A dropped connection or a timeout is retried and resumed with "Range: bytes=<done>-"
    // (Microsoft CDN and Google Drive both support it), so a hiccup does not restart 300 MB.
    const int MAX_ATTEMPTS = 6;
    std::vector<char> buf(256 * 1024);
    uint64_t done = 0, total = 0;
    bool ok = false, fatal = false;
    for (int attempt = 0; attempt < MAX_ATTEMPTS && !ok && !fatal; ++attempt) {
        if (attempt > 0) {
            for (int t = 0; t < attempt * 10 && !(cancel && cancel->load()); ++t) Sleep(200);   // 2 s, 4 s, ...
        }
        if (cancel && cancel->load()) { status = L"загрузка отменена"; break; }

        std::wstring headers;
        if (done > 0) headers = L"Range: bytes=" + std::to_wstring(done) + L"-\r\n";
        InetHandle req(InternetOpenUrlW(session, url.c_str(), headers.empty() ? nullptr : headers.c_str(),
                                        headers.empty() ? 0 : (DWORD)-1L, RequestFlags(url), 0));
        if (!req) { status = ErrorText(GetLastError()); continue; }
        if (!CheckStatus(req, status)) { fatal = true; break; }

        DWORD code = 0, len = sizeof(code);
        HttpQueryInfoW(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &len, nullptr);
        if (done > 0 && code != 206) {
            // The server ignored Range: start the file over.
            SetFilePointer(file, 0, nullptr, FILE_BEGIN);
            SetEndOfFile(file);
            done = 0;
        }
        uint64_t length = ContentLength(req);
        if (length) total = done + length;

        bool finished = false;
        DWORD read = 0;
        for (;;) {
            if (cancel && cancel->load()) { status = L"загрузка отменена"; fatal = true; break; }
            if (!InternetReadFile(req, buf.data(), (DWORD)buf.size(), &read)) {
                status = L"обрыв загрузки: " + ErrorText(GetLastError());
                break;   // retry and resume
            }
            if (read == 0) { finished = true; break; }
            DWORD written = 0;
            if (!WriteFile(file, buf.data(), read, &written, nullptr) || written != read) {
                status = L"ошибка записи на диск (нет места?)";
                fatal = true;
                break;
            }
            done += read;
            if (onBytes) onBytes(done, total);
        }
        if (finished) {
            if (total && done != total) status = L"файл скачан не полностью";   // retry and resume
            else ok = true;
        }
    }
    CloseHandle(file);

    if (!ok) { DeleteFileW(part.c_str()); return false; }
    if (!MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(part.c_str());
        status = L"не удалось сохранить загруженный файл";
        return false;
    }
    status = L"OK";
    return true;
}

std::string Sha256File(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::string result;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        std::vector<UCHAR> buf(1024 * 1024);
        DWORD read = 0;
        bool ok = true;
        while (ReadFile(file, buf.data(), (DWORD)buf.size(), &read, nullptr) && read > 0) {
            if (BCryptHashData(hash, buf.data(), read, 0) != 0) { ok = false; break; }
        }
        UCHAR digest[32];
        if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
            static const char* hex = "0123456789abcdef";
            for (UCHAR b : digest) { result += hex[b >> 4]; result += hex[b & 15]; }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(file);
    return result;
}

} // namespace Net
