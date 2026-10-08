// Compiled without the precompiled header.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

#include "keypatch.h"

namespace fs = std::filesystem;

namespace {

// Keys from KeyPatcher (src/main.cpp, namespace KeyData).
constexpr std::string_view OLD_ROOT_PUBLIC_KEY{
    "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE8ELkixyLcwlZryUQcu1TvPOmI2B7vX83ndnWRUaXm74wFfa5f/lwQNTfrLVHa2PmenpGI6JhIMUJaWZrjmMj90NoKNFSNBuKdm8rYiXsfaz3K36x/1U26HpG0ZxK/V1V"
};
constexpr std::string_view NEW_ROOT_PUBLIC_KEY{
    "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAECRXueJeTDqNRRgJi/vlRufByu/2G0i2Ebt6YMar5QX/R0DIIyrJMcUpruK4QveTfJSTp3Shlq4Gk34cD/4GUWwkv0DVuzeuB+tXija7HBxii03NHDbPAD0AKnLr2wdAp"
};
static_assert(OLD_ROOT_PUBLIC_KEY.size() == NEW_ROOT_PUBLIC_KEY.size());

std::vector<size_t> FindAll(const std::vector<char>& data, std::string_view needle) {
    std::vector<size_t> hits;
    std::boyer_moore_horspool_searcher searcher(needle.begin(), needle.end());
    auto it = data.begin();
    while (true) {
        it = std::search(it, data.end(), searcher);
        if (it == data.end()) break;
        hits.push_back((size_t)(it - data.begin()));
        it += needle.size();
    }
    return hits;
}

bool ReadWholeFile(HANDLE h, std::vector<char>& data) {
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > (1ll << 31)) return false;
    data.resize((size_t)size.QuadPart);
    size_t done = 0;
    while (done < data.size()) {
        DWORD read = 0;
        DWORD chunk = (DWORD)std::min<size_t>(data.size() - done, 16u * 1024 * 1024);
        if (!ReadFile(h, data.data() + done, chunk, &read, nullptr) || read == 0) return false;
        done += read;
    }
    return true;
}

} // namespace

namespace KeyPatch {

std::wstring FindExecutable(const std::wstring& gameDir) {
    fs::path direct = fs::path(gameDir) / L"Minecraft.Windows.exe";
    std::error_code ec;
    if (fs::is_regular_file(direct, ec)) return direct.wstring();
    for (auto it = fs::recursive_directory_iterator(gameDir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_regular_file(ec) && _wcsicmp(it->path().filename().c_str(), L"Minecraft.Windows.exe") == 0)
            return it->path().wstring();
    }
    return {};
}

Result Patch(const std::wstring& exePath, std::wstring& status) {
    HANDLE h = CreateFileW(exePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        status = (err == ERROR_SHARING_VIOLATION)
            ? L"Minecraft.Windows.exe занят — закройте игру и попробуйте снова"
            : L"нет доступа к Minecraft.Windows.exe (код " + std::to_wstring(err) + L")";
        return Result::Error;
    }

    std::vector<char> data;
    if (!ReadWholeFile(h, data)) {
        CloseHandle(h);
        status = L"не удалось прочитать Minecraft.Windows.exe";
        return Result::Error;
    }

    std::vector<size_t> hits = FindAll(data, OLD_ROOT_PUBLIC_KEY);
    if (hits.empty()) {
        CloseHandle(h);
        if (!FindAll(data, NEW_ROOT_PUBLIC_KEY).empty()) {
            status = L"Xbox Live уже пропатчен";
            return Result::AlreadyPatched;
        }
        status = L"ключ Xbox Live не найден: версия старше Xbox-авторизации или использует другой ключ";
        return Result::KeyNotFound;
    }

    // KeyPatcher patches the first match; every match is patched here in case a build has several.
    for (size_t offset : hits) {
        LARGE_INTEGER pos{};
        pos.QuadPart = (LONGLONG)offset;
        DWORD written = 0;
        if (!SetFilePointerEx(h, pos, nullptr, FILE_BEGIN) ||
            !WriteFile(h, NEW_ROOT_PUBLIC_KEY.data(), (DWORD)NEW_ROOT_PUBLIC_KEY.size(), &written, nullptr) ||
            written != NEW_ROOT_PUBLIC_KEY.size()) {
            CloseHandle(h);
            status = L"не удалось записать патч в Minecraft.Windows.exe";
            return Result::Error;
        }
    }
    FlushFileBuffers(h);
    CloseHandle(h);

    wchar_t hex[32];
    swprintf_s(hex, L"0x%zx", hits.front());
    status = L"Xbox Live пропатчен (ключ по смещению " + std::wstring(hex) + L")";
    return Result::Patched;
}

} // namespace KeyPatch
