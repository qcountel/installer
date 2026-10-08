// .appx extraction with miniz. All file access goes through Win32 wide-char APIs,
// so Cyrillic and long paths work. Compiled without the precompiled header.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "miniz.h"

#include "appx.h"
#include "net.h"

namespace fs = std::filesystem;

namespace {

// \\?\ prefix lifts the MAX_PATH limit for deep game folders.
std::wstring LongPath(const std::wstring& p) {
    if (p.rfind(L"\\\\?\\", 0) == 0) return p;
    if (p.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + p.substr(2);
    return L"\\\\?\\" + p;
}

size_t ReadAt(void* opaque, mz_uint64 offset, void* buf, size_t n) {
    HANDLE h = *static_cast<HANDLE*>(opaque);
    size_t total = 0;
    while (total < n) {
        OVERLAPPED ov{};
        mz_uint64 pos = offset + total;
        ov.Offset = (DWORD)(pos & 0xFFFFFFFFull);
        ov.OffsetHigh = (DWORD)(pos >> 32);
        DWORD chunk = (DWORD)std::min<size_t>(n - total, 64u * 1024 * 1024);
        DWORD read = 0;
        if (!ReadFile(h, static_cast<char*>(buf) + total, chunk, &read, &ov) || read == 0) break;
        total += read;
    }
    return total;
}

struct WriteCtx {
    HANDLE file;
    uint64_t* done;
    uint64_t total;
    const std::function<void(uint64_t, uint64_t)>* onProgress;
    const std::atomic<bool>* cancel;
    DWORD lastTick;
};

size_t WriteChunk(void* opaque, mz_uint64 /*offset*/, const void* buf, size_t n) {
    auto* ctx = static_cast<WriteCtx*>(opaque);
    if (ctx->cancel && ctx->cancel->load()) return 0;   // makes miniz abort
    DWORD written = 0;
    if (!WriteFile(ctx->file, buf, (DWORD)n, &written, nullptr) || written != n) return 0;
    *ctx->done += n;
    DWORD now = GetTickCount();
    if (ctx->onProgress && *ctx->onProgress && now - ctx->lastTick >= 150) {
        ctx->lastTick = now;
        (*ctx->onProgress)(*ctx->done, ctx->total);
    }
    return n;
}

int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// OPC part names are percent-encoded ("%20" for a space); decode them like MakeAppx does.
std::string PercentDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = HexVal(s[i + 1]), lo = HexVal(s[i + 2]);
            if (hi >= 0 && lo >= 0) { out += (char)(hi * 16 + lo); i += 2; continue; }
        }
        out += s[i];
    }
    return out;
}

// Rejects absolute paths and ".." so a malicious package cannot write outside dir.
bool SafeRelative(const std::wstring& rel) {
    if (rel.empty() || rel[0] == L'\\' || rel[0] == L'/' || rel.find(L':') != std::wstring::npos) return false;
    size_t start = 0;
    while (start <= rel.size()) {
        size_t end = rel.find(L'\\', start);
        if (end == std::wstring::npos) end = rel.size();
        if (rel.compare(start, end - start, L"..") == 0) return false;
        start = end + 1;
    }
    return true;
}

} // namespace

namespace Appx {

bool Extract(const std::wstring& appxPath, const std::wstring& dir,
             const std::function<void(uint64_t, uint64_t)>& onProgress,
             const std::atomic<bool>* cancel, std::wstring& status) {
    HANDLE src = CreateFileW(appxPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                             FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (src == INVALID_HANDLE_VALUE) { status = L"не удалось открыть скачанный пакет"; return false; }
    LARGE_INTEGER size{};
    GetFileSizeEx(src, &size);

    mz_zip_archive zip{};
    zip.m_pRead = ReadAt;
    zip.m_pIO_opaque = &src;
    if (!mz_zip_reader_init(&zip, (mz_uint64)size.QuadPart, 0)) {
        CloseHandle(src);
        status = L"пакет повреждён (не удалось прочитать архив)";
        return false;
    }

    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    uint64_t total = 0, done = 0;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (mz_zip_reader_file_stat(&zip, i, &st)) total += st.m_uncomp_size;
    }

    bool ok = true;
    std::error_code ec;
    fs::create_directories(LongPath(dir), ec);
    DWORD lastTick = 0;

    for (mz_uint i = 0; i < count && ok; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st)) { status = L"пакет повреждён"; ok = false; break; }

        std::wstring rel = Net::Widen(PercentDecode(st.m_filename));
        for (auto& c : rel) if (c == L'/') c = L'\\';
        while (!rel.empty() && rel.back() == L'\\') rel.pop_back();
        if (rel.empty()) continue;
        if (!SafeRelative(rel)) { status = L"в пакете недопустимый путь: " + rel; ok = false; break; }
        if (_wcsicmp(rel.c_str(), L"AppxSignature.p7x") == 0) continue;   // unsigned = Developer Mode

        std::wstring target = LongPath(dir + L"\\" + rel);
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            fs::create_directories(target, ec);
            continue;
        }
        fs::create_directories(fs::path(target).parent_path(), ec);

        HANDLE out = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE) {
            status = L"не удалось создать файл " + rel + L" (код " + std::to_wstring(GetLastError()) + L")";
            ok = false;
            break;
        }
        WriteCtx ctx{ out, &done, total, &onProgress, cancel, lastTick };
        bool fileOk = mz_zip_reader_extract_to_callback(&zip, i, WriteChunk, &ctx, 0) != 0;
        lastTick = ctx.lastTick;
        CloseHandle(out);
        if (!fileOk) {
            DeleteFileW(target.c_str());
            if (cancel && cancel->load()) status = L"распаковка отменена";
            else status = L"не удалось распаковать " + rel + L": " +
                          Net::Widen(mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
            ok = false;
        }
    }

    mz_zip_reader_end(&zip);
    CloseHandle(src);
    if (ok && onProgress) onProgress(total, total);
    return ok;
}

namespace {

bool OpenZip(const std::wstring& path, HANDLE& h, mz_zip_archive& zip, std::wstring& status) {
    h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (h == INVALID_HANDLE_VALUE) { status = L"не удалось открыть архив"; return false; }
    LARGE_INTEGER size{};
    GetFileSizeEx(h, &size);
    zip = mz_zip_archive{};
    zip.m_pRead = ReadAt;
    zip.m_pIO_opaque = &h;
    if (!mz_zip_reader_init(&zip, (mz_uint64)size.QuadPart, 0)) {
        CloseHandle(h);
        status = L"архив повреждён (не удалось прочитать zip)";
        return false;
    }
    return true;
}

bool EndsWithNoCase(const std::wstring& s, const std::wstring& suffix) {
    if (s.size() < suffix.size()) return false;
    return _wcsicmp(s.c_str() + (s.size() - suffix.size()), suffix.c_str()) == 0;
}

} // namespace

bool ExtractEntry(const std::wstring& zipPath, const std::wstring& suffix, const std::wstring& outPath,
                  const std::function<void(uint64_t, uint64_t)>& onProgress,
                  const std::atomic<bool>* cancel, std::wstring& foundName, std::wstring& status) {
    HANDLE src = INVALID_HANDLE_VALUE;
    mz_zip_archive zip{};
    if (!OpenZip(zipPath, src, zip, status)) return false;

    bool ok = false;
    status = L"в архиве нет файла *" + suffix;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st) || mz_zip_reader_is_file_a_directory(&zip, i)) continue;
        std::wstring name = Net::Widen(st.m_filename);
        if (!EndsWithNoCase(name, suffix)) continue;
        foundName = name;

        std::error_code ec;
        fs::create_directories(fs::path(outPath).parent_path(), ec);
        HANDLE out = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE) { status = L"не удалось создать " + outPath; break; }
        uint64_t done = 0;
        WriteCtx ctx{ out, &done, st.m_uncomp_size, &onProgress, cancel, 0 };
        ok = mz_zip_reader_extract_to_callback(&zip, i, WriteChunk, &ctx, 0) != 0;
        CloseHandle(out);
        if (!ok) {
            DeleteFileW(outPath.c_str());
            status = (cancel && cancel->load()) ? L"распаковка отменена"
                   : L"не удалось распаковать " + name + L": " + Net::Widen(mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
        } else {
            status.clear();
            if (onProgress) onProgress(st.m_uncomp_size, st.m_uncomp_size);
        }
        break;
    }
    mz_zip_reader_end(&zip);
    CloseHandle(src);
    return ok;
}

bool ReadEntry(const std::wstring& zipPath, const char* name, std::string& out, std::wstring& status) {
    HANDLE src = INVALID_HANDLE_VALUE;
    mz_zip_archive zip{};
    if (!OpenZip(zipPath, src, zip, status)) return false;
    size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
    bool ok = data != nullptr;
    if (ok) out.assign(static_cast<const char*>(data), size);
    else status = L"в пакете нет " + Net::Widen(name);
    mz_free(data);
    mz_zip_reader_end(&zip);
    CloseHandle(src);
    return ok;
}

} // namespace Appx
