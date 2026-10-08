// NOTE: compiled WITHOUT the precompiled header (see src/CMakeLists.txt) so that
// C++/WinRT headers do not clash with wxWidgets macros.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Core.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Storage.h>

#include <chrono>
#include <ctime>
#include <filesystem>

#include "installer.h"
#include "config.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "windowsapp.lib")

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Management::Deployment;
namespace fs = std::filesystem;

namespace {

void EnsureApartment() {
    try { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    catch (...) { /* already initialised on this thread (e.g. the UI thread is STA) */ }
}

std::wstring Hex(HRESULT hr) {
    wchar_t buf[16];
    swprintf_s(buf, L"0x%08X", (unsigned)hr);
    return buf;
}

std::wstring Hint(HRESULT hr) {
    switch ((unsigned)hr) {
    case 0x80073CFF: return L"включите режим разработчика: Параметры → Для разработчиков";
    case 0x80073CF3: return L"не хватает зависимостей (Microsoft.VCLibs) или конфликт версий";
    case 0x80073CFB: return L"уже установлен пакет той же версии из другой папки — удалите его";
    case 0x80073D02: return L"игра сейчас запущена — закройте её";
    case 0x80073D06: return L"для другого пользователя установлена более новая версия Minecraft";
    case 0x80073CF9: return L"установка не удалась (нет места на диске или ошибка Windows)";
    case 0x80070005: return L"нет доступа — запустите установщик от имени администратора";
    case 0x80070003: return L"путь не найден — папка версии повреждена, удалите её и скачайте снова";
    default: return L"";
    }
}

std::wstring PathToFileUri(const std::wstring& path) {
    std::wstring uri = L"file:///";
    for (wchar_t c : path) {
        if (c == L'\\') uri += L'/';
        else if (c == L' ') uri += L"%20";
        else if (c == L'%') uri += L"%25";
        else if (c == L'#') uri += L"%23";
        else uri += c;
    }
    return uri;
}

std::wstring LongPath(const std::wstring& p) {
    return p.rfind(L"\\\\", 0) == 0 ? p : L"\\\\?\\" + p;
}

bool SamePath(const std::wstring& a, const std::wstring& b) {
    std::error_code ec;
    if (a.empty() || b.empty()) return false;
    if (fs::equivalent(a, b, ec)) return true;
    return _wcsicmp(fs::path(a).lexically_normal().c_str(), fs::path(b).lexically_normal().c_str()) == 0;
}

std::wstring LocalAppData() {
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) out = p;
    CoTaskMemFree(p);
    return out;
}

std::wstring StoreWorldsDir() {
    return LocalAppData() + L"\\Packages\\" + Globals::PACKAGE_FAMILY + L"\\LocalState\\games\\com.mojang";
}

std::wstring Timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    wchar_t buf[32];
    wcsftime(buf, 32, L"%Y%m%d_%H%M%S", &tm);
    return buf;
}

bool CopyTree(const std::wstring& from, const std::wstring& to, std::wstring& status) {
    std::error_code ec;
    fs::create_directories(LongPath(to), ec);
    fs::copy(LongPath(from), LongPath(to), fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::string msg = ec.message();   // system ANSI code page
        int n = MultiByteToWideChar(CP_ACP, 0, msg.data(), (int)msg.size(), nullptr, 0);
        std::wstring wmsg(n, L'\0');
        MultiByteToWideChar(CP_ACP, 0, msg.data(), (int)msg.size(), wmsg.data(), n);
        status = L"ошибка копирования миров: " + wmsg;
        return false;
    }
    return true;
}

bool Wait(IAsyncOperationWithProgress<DeploymentResult, DeploymentProgress> op,
          const std::function<void(unsigned)>& onProgress, std::wstring& status) {
    op.Progress([&](auto const&, DeploymentProgress const& p) {
        if (onProgress) onProgress(p.percentage);
    });
    AsyncStatus st = op.wait_for(std::chrono::hours(2));
    if (st == AsyncStatus::Completed) return true;

    HRESULT hr = op.ErrorCode();
    std::wstring text;
    try {
        DeploymentResult res = op.GetResults();
        hr = res.ExtendedErrorCode();
        text = res.ErrorText().c_str();
    } catch (winrt::hresult_error const& e) {
        hr = e.code();
        text = e.message().c_str();
    }
    std::wstring hint = Hint(hr);
    status = Hex(hr) + (hint.empty() ? L"" : L": " + hint);
    if (!text.empty()) status += L"\n" + text;
    return false;
}

} // namespace

namespace Installer {

bool IsDeveloperModeEnabled() {
    DWORD value = 0, size = sizeof(value);
    LSTATUS rc = RegGetValueW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock",
        L"AllowDevelopmentWithoutDevLicense", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return rc == ERROR_SUCCESS && value == 1;
}

void OpenDeveloperSettings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:developers", nullptr, nullptr, SW_SHOWNORMAL);
}

std::wstring RegisteredLocation() {
    EnsureApartment();
    try {
        PackageManager pm;
        for (auto const& pkg : pm.FindPackagesForUser(winrt::hstring{}, Globals::PACKAGE_FAMILY)) {
            try { return std::wstring(pkg.InstalledLocation().Path().c_str()); }
            catch (...) {}
        }
    } catch (...) {}
    return {};
}

bool IsGameRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{ sizeof(pe) };
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, L"Minecraft.Windows.exe") == 0) { found = true; break; }
    }
    CloseHandle(snap);
    return found;
}

bool Register(const std::wstring& gameDir, const std::wstring& backupRoot,
              const std::function<void(unsigned)>& onProgress,
              const std::function<void(const std::wstring&)>& onStatus,
              std::wstring& status) {
    EnsureApartment();
    std::wstring manifest = gameDir + L"\\AppxManifest.xml";
    if (GetFileAttributesW(manifest.c_str()) == INVALID_FILE_ATTRIBUTES) {
        status = L"в папке версии нет AppxManifest.xml — удалите папку и скачайте снова";
        return false;
    }

    std::wstring restoreFrom;
    try {
        PackageManager pm;
        for (auto const& pkg : pm.FindPackagesForUser(winrt::hstring{}, Globals::PACKAGE_FAMILY)) {
            std::wstring location;
            try { location = pkg.InstalledLocation().Path().c_str(); } catch (...) {}
            if (SamePath(location, gameDir)) {
                status = L"Эта версия уже зарегистрирована";
                return true;
            }

            if (!pkg.IsDevelopmentMode()) {
                // A Store install: removing it deletes its data, so copy the worlds out first.
                std::wstring worlds = StoreWorldsDir();
                std::error_code ec;
                if (fs::exists(LongPath(worlds), ec)) {
                    std::wstring backup = backupRoot + L"\\com.mojang_" + Timestamp();
                    if (onStatus) onStatus(L"Сохраняю миры и настройки в " + backup + L"...");
                    if (!CopyTree(worlds, backup, status)) return false;
                    restoreFrom = backup;
                }
                if (onStatus) onStatus(L"Удаляю Minecraft из Store (миры сохранены)...");
                if (!Wait(pm.RemovePackageAsync(pkg.Id().FullName()), onProgress, status)) {
                    status = L"Не удалось удалить текущий Minecraft: " + status;
                    return false;
                }
            } else {
                // Another unpacked version: its data stays in place.
                if (onStatus) onStatus(L"Отключаю предыдущую версию...");
                if (!Wait(pm.RemovePackageAsync(pkg.Id().FullName(), RemovalOptions::PreserveApplicationData),
                          onProgress, status)) {
                    status = L"Не удалось отключить предыдущую версию: " + status;
                    return false;
                }
            }
        }

        if (onStatus) onStatus(L"Регистрирую игру в Windows...");
        Uri uri{ winrt::hstring(PathToFileUri(manifest)) };
        if (!Wait(pm.RegisterPackageAsync(uri, nullptr, DeploymentOptions::DevelopmentMode), onProgress, status)) {
            status = L"Ошибка регистрации " + status +
                     (restoreFrom.empty() ? L"" : L"\nМиры сохранены в " + restoreFrom);
            return false;
        }
    } catch (winrt::hresult_error const& e) {
        std::wstring hint = Hint(e.code());
        status = L"Ошибка регистрации " + Hex(e.code()) + L": " +
                 (hint.empty() ? std::wstring(e.message().c_str()) : hint);
        return false;
    }

    if (!restoreFrom.empty()) {
        if (onStatus) onStatus(L"Возвращаю миры и настройки...");
        try {
            auto data = winrt::Windows::Management::Core::ApplicationDataManager::CreateForPackageFamily(
                Globals::PACKAGE_FAMILY);
            std::wstring target = std::wstring(data.LocalFolder().Path().c_str()) + L"\\games\\com.mojang";
            std::wstring err;
            if (!CopyTree(restoreFrom, target, err)) {
                status = L"Игра установлена, но миры не вернулись: " + err + L"\nКопия: " + restoreFrom;
                return true;
            }
        } catch (winrt::hresult_error const& e) {
            status = L"Игра установлена, но миры не вернулись (" + Hex(e.code()) + L")\nКопия: " + restoreFrom;
            return true;
        }
    }

    status = L"Игра зарегистрирована";
    return true;
}

bool Launch() {
    std::wstring target = std::wstring(L"shell:AppsFolder\\") + Globals::APP_ID;
    HINSTANCE r = ShellExecuteW(nullptr, L"open", L"explorer.exe", target.c_str(), nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(r) > 32;
}

void OpenFolder(const std::wstring& dir) {
    CreateDirectoryW(dir.c_str(), nullptr);
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace Installer
