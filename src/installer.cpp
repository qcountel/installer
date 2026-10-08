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
#include <wincrypt.h>
#include <tlhelp32.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Core.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "installer.h"
#include "config.h"
#include "net.h"
#include "wu.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "crypt32.lib")

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
    case 0x80073CFF: return L"Windows запрещает такую установку: включите режим разработчика (Параметры → Для разработчиков)";
    case 0x80073CF3: return L"не хватает зависимостей (Microsoft.VCLibs / Store.Engagement) или конфликт версий";
    case 0x80073CFB: return L"уже установлен пакет той же версии из другой папки — удалите его";
    case 0x80073D02: return L"игра сейчас запущена — закройте её";
    case 0x80073D06: return L"для другого пользователя установлена более новая версия Minecraft";
    case 0x800B0109: return L"сертификат сборки не доверенный — его не удалось добавить в систему";
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


// Framework packages Minecraft (UWP) depends on. Exact x64 builds served by Windows Update
// (UpdateIDs were taken from the Store listings, size and SHA-256 from the downloaded files).
struct Framework {
    const wchar_t* name;
    uint16_t       version[4];
    const wchar_t* updateId;
    uint64_t       size;
    const char*    sha256;
};

const Framework FRAMEWORKS[] = {
    { L"Microsoft.VCLibs.140.00", { 14, 0, 33519, 0 },
      L"6194cec0-df15-4b08-af05-b09565805255", 896581ull,
      "9c17b521f9d690a1f504da5108ed6eec5669eb3a8fd1331eef43e40d84e74283" },
    { L"Microsoft.Services.Store.Engagement", { 10, 0, 23012, 0 },
      L"e2ebdeec-c7ae-4b55-9dfa-e855bb734c58", 299159ull,
      "0133757628606a9c73f6265e235ad6fb6e80973b89a04b8c0208f93786a24f93" },
};

const wchar_t* MS_PUBLISHER = L"CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US";

uint64_t PackVersion(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
    return ((uint64_t)a << 48) | ((uint64_t)b << 32) | ((uint64_t)c << 16) | d;
}

uint64_t ParseVersion(const std::string& s) {
    unsigned v[4] = { 0, 0, 0, 0 };
    sscanf_s(s.c_str(), "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]);
    return PackVersion((uint16_t)v[0], (uint16_t)v[1], (uint16_t)v[2], (uint16_t)v[3]);
}

// MinVersion of <PackageDependency Name="name" .../> in the manifest; false if not listed.
bool ManifestDependency(const std::string& manifest, const std::wstring& name, uint64_t& minVersion) {
    std::string key = "Name=\"" + Net::Narrow(name) + "\"";   // closing quote: skip *.UWPDesktop
    size_t pos = 0;
    while ((pos = manifest.find("<PackageDependency", pos)) != std::string::npos) {
        size_t end = manifest.find('>', pos);
        if (end == std::string::npos) break;
        std::string tag = manifest.substr(pos, end - pos);
        pos = end;
        if (tag.find(key) == std::string::npos) continue;
        minVersion = 0;
        size_t mv = tag.find("MinVersion=\"");
        if (mv != std::string::npos) {
            mv += 12;
            minVersion = ParseVersion(tag.substr(mv, tag.find('"', mv) - mv));
        }
        return true;
    }
    return false;
}

// Highest installed x64 (or neutral) version of a framework for the current user, 0 if none.
uint64_t InstalledVersion(PackageManager& pm, const std::wstring& name) {
    uint64_t best = 0;
    try {
        for (auto const& pkg : pm.FindPackagesForUser(winrt::hstring{}, name, MS_PUBLISHER)) {
            auto id = pkg.Id();
            auto arch = id.Architecture();
            if (arch != winrt::Windows::System::ProcessorArchitecture::X64 &&
                arch != winrt::Windows::System::ProcessorArchitecture::Neutral) continue;
            auto v = id.Version();
            best = (std::max)(best, PackVersion(v.Major, v.Minor, v.Build, v.Revision));
        }
    } catch (...) {}
    return best;
}

std::wstring VersionString(const uint16_t v[4]) {
    return std::to_wstring(v[0]) + L"." + std::to_wstring(v[1]) + L"." + std::to_wstring(v[2]) + L"." + std::to_wstring(v[3]);
}

uint64_t FileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return 0;
    return ((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow;
}


// Removes the Minecraft package currently installed for this user, unless it already points at
// keepLocation (then `already` is set). A Store / signed install is removed together with its data,
// so its worlds are copied to backupRoot first (restoreFrom receives the copy).
bool RemoveCurrent(PackageManager& pm, const std::wstring& keepLocation, const std::wstring& backupRoot,
                   const std::function<void(unsigned)>& onProgress,
                   const std::function<void(const std::wstring&)>& onStatus,
                   std::wstring& restoreFrom, bool& already, std::wstring& status) {
    already = false;
    for (auto const& pkg : pm.FindPackagesForUser(winrt::hstring{}, Globals::PACKAGE_FAMILY)) {
        std::wstring location;
        try { location = pkg.InstalledLocation().Path().c_str(); } catch (...) {}
        if (!keepLocation.empty() && SamePath(location, keepLocation)) { already = true; return true; }

        if (!pkg.IsDevelopmentMode()) {
            std::wstring worlds = StoreWorldsDir();
            std::error_code ec;
            if (fs::exists(LongPath(worlds), ec)) {
                std::wstring backup = backupRoot + L"\\com.mojang_" + Timestamp();
                if (onStatus) onStatus(L"Сохраняю миры и настройки в " + backup + L"...");
                if (!CopyTree(worlds, backup, status)) return false;
                restoreFrom = backup;
            }
            if (onStatus) onStatus(L"Удаляю текущий Minecraft (миры сохранены)...");
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
    return true;
}

// Copies backed-up worlds into the freshly installed package; appends a note to status on failure.
void RestoreWorlds(const std::wstring& restoreFrom,
                   const std::function<void(const std::wstring&)>& onStatus, std::wstring& status) {
    if (restoreFrom.empty()) return;
    if (onStatus) onStatus(L"Возвращаю миры и настройки...");
    try {
        auto data = winrt::Windows::Management::Core::ApplicationDataManager::CreateForPackageFamily(
            Globals::PACKAGE_FAMILY);
        std::wstring target = std::wstring(data.LocalFolder().Path().c_str()) + L"\\games\\com.mojang";
        std::wstring err;
        if (!CopyTree(restoreFrom, target, err))
            status += L"\nМиры не вернулись: " + err + L"\nКопия: " + restoreFrom;
    } catch (winrt::hresult_error const& e) {
        status += L"\nМиры не вернулись (" + Hex(e.code()) + L")\nКопия: " + restoreFrom;
    }
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

bool EnsureDependencies(const std::wstring& gameDir, const std::wstring& depsDir,
                        const std::function<void(const std::wstring&)>& onStatus,
                        std::wstring& status) {
    std::string manifest;
    {
        std::ifstream f(fs::path(LongPath(gameDir + L"\\AppxManifest.xml")), std::ios::binary);
        if (!f) { status = L"в папке версии нет AppxManifest.xml — удалите папку и скачайте снова"; return false; }
        std::ostringstream ss; ss << f.rdbuf(); manifest = ss.str();
    }
    return EnsureDependenciesFor(manifest, depsDir, onStatus, status);
}

bool EnsureDependenciesFor(const std::string& manifest, const std::wstring& depsDir,
                           const std::function<void(const std::wstring&)>& onStatus,
                           std::wstring& status) {
    EnsureApartment();
    try {
        PackageManager pm;
        for (const Framework& fw : FRAMEWORKS) {
            uint64_t need = 0;
            if (!ManifestDependency(manifest, fw.name, need)) continue;
            if (InstalledVersion(pm, fw.name) >= need) continue;

            const uint64_t ours = PackVersion(fw.version[0], fw.version[1], fw.version[2], fw.version[3]);
            std::wstring label = std::wstring(fw.name) + L" " + VersionString(fw.version);
            if (ours < need) {
                status = L"нужна более новая " + std::wstring(fw.name) + L" — установите её из Microsoft Store";
                return false;
            }

            CreateDirectoryW(depsDir.c_str(), nullptr);
            std::wstring appx = depsDir + L"\\" + fw.name + L"_" + VersionString(fw.version) + L"_x64.appx";
            bool have = FileSize(appx) == fw.size && Net::Sha256File(appx) == fw.sha256;
            if (!have) {
                if (onStatus) onStatus(L"Скачиваю зависимость " + label + L"...");
                std::wstring url, err;
                if (!WU::ResolveDownloadUrl(fw.updateId, url, err)) {
                    status = L"не удалось получить " + label + L": " + err;
                    return false;
                }
                if (!Net::DownloadFile(url, appx, nullptr, nullptr, err)) {
                    status = L"не удалось скачать " + label + L": " + err;
                    return false;
                }
                if (FileSize(appx) != fw.size || Net::Sha256File(appx) != fw.sha256) {
                    DeleteFileW(appx.c_str());
                    status = L"файл " + label + L" повреждён (не совпал SHA-256), попробуйте ещё раз";
                    return false;
                }
            }

            if (onStatus) onStatus(L"Устанавливаю зависимость " + label + L"...");
            Uri uri{ winrt::hstring(PathToFileUri(appx)) };
            std::wstring err;
            if (!Wait(pm.AddPackageAsync(uri, nullptr, DeploymentOptions::None), nullptr, err)) {
                status = L"не удалось установить " + label + L": " + err;
                return false;
            }
        }
    } catch (winrt::hresult_error const& e) {
        status = L"ошибка установки зависимостей " + Hex(e.code()) + L": " + std::wstring(e.message().c_str());
        return false;
    }
    return true;
}

PackageState Current() {
    EnsureApartment();
    PackageState st;
    try {
        PackageManager pm;
        for (auto const& pkg : pm.FindPackagesForUser(winrt::hstring{}, Globals::PACKAGE_FAMILY)) {
            st.found = true;
            try { st.location = pkg.InstalledLocation().Path().c_str(); } catch (...) {}
            try { st.devMode = pkg.IsDevelopmentMode(); } catch (...) {}
            auto v = pkg.Id().Version();
            st.version = std::to_wstring(v.Major) + L"." + std::to_wstring(v.Minor) + L"." +
                         std::to_wstring(v.Build) + L"." + std::to_wstring(v.Revision);
            break;
        }
    } catch (...) {}
    return st;
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
        bool already = false;
        if (!RemoveCurrent(pm, gameDir, backupRoot, onProgress, onStatus, restoreFrom, already, status)) return false;
        if (already) { status = L"Эта версия уже зарегистрирована"; return true; }

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

    status = L"Игра зарегистрирована";
    RestoreWorlds(restoreFrom, onStatus, status);
    return true;
}

bool AddTrustedCertificate(const std::wstring& cerPath, std::wstring& status) {
    std::string data;
    {
        std::ifstream f(fs::path(cerPath), std::ios::binary);
        if (!f) { status = L"не удалось открыть сертификат"; return false; }
        std::ostringstream ss; ss << f.rdbuf(); data = ss.str();
    }
    // .cer is usually DER; accept Base64/PEM too.
    std::vector<BYTE> der(data.begin(), data.end());
    if (data.rfind("-----BEGIN", 0) == 0 || (data.size() > 2 && data[0] == 'M' && data[1] == 'I')) {
        DWORD n = 0;
        if (CryptStringToBinaryA(data.c_str(), (DWORD)data.size(), CRYPT_STRING_BASE64_ANY, nullptr, &n, nullptr, nullptr)) {
            der.resize(n);
            CryptStringToBinaryA(data.c_str(), (DWORD)data.size(), CRYPT_STRING_BASE64_ANY, der.data(), &n, nullptr, nullptr);
        }
    }

    // Local Machine \ Trusted People: what Windows checks for sideloaded (self-signed) packages.
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                                     CERT_SYSTEM_STORE_LOCAL_MACHINE, L"TrustedPeople");
    if (!store) { status = L"нет доступа к хранилищу сертификатов (" + Hex(HRESULT_FROM_WIN32(GetLastError())) + L")"; return false; }
    BOOL ok = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                               der.data(), (DWORD)der.size(), CERT_STORE_ADD_REPLACE_EXISTING, nullptr);
    DWORD err = GetLastError();
    CertCloseStore(store, 0);
    if (!ok) { status = L"сертификат не принят Windows (" + Hex(HRESULT_FROM_WIN32(err)) + L")"; return false; }
    return true;
}

bool InstallSigned(const std::wstring& appxPath, const std::wstring& cerPath, const std::wstring& backupRoot,
                   const std::function<void(unsigned)>& onProgress,
                   const std::function<void(const std::wstring&)>& onStatus,
                   std::wstring& status) {
    EnsureApartment();
    if (onStatus) onStatus(L"Добавляю сертификат сборки в «Доверенные лица» (локальный компьютер)...");
    if (!AddTrustedCertificate(cerPath, status)) { status = L"Сертификат: " + status; return false; }

    std::wstring restoreFrom;
    try {
        PackageManager pm;
        bool already = false;
        if (!RemoveCurrent(pm, L"", backupRoot, onProgress, onStatus, restoreFrom, already, status)) return false;

        if (onStatus) onStatus(L"Устанавливаю подписанную сборку...");
        Uri uri{ winrt::hstring(PathToFileUri(appxPath)) };
        if (!Wait(pm.AddPackageAsync(uri, nullptr, DeploymentOptions::ForceApplicationShutdown), onProgress, status)) {
            status = L"Ошибка установки " + status +
                     (restoreFrom.empty() ? L"" : L"\nМиры сохранены в " + restoreFrom);
            return false;
        }
    } catch (winrt::hresult_error const& e) {
        std::wstring hint = Hint(e.code());
        status = L"Ошибка установки " + Hex(e.code()) + L": " +
                 (hint.empty() ? std::wstring(e.message().c_str()) : hint);
        return false;
    }

    status = L"Игра установлена";
    RestoreWorlds(restoreFrom, onStatus, status);
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
