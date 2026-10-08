#include "pch.h"
#include "cApp.h"

#include <shlobj.h>

wxIMPLEMENT_APP(cApp);

namespace {

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &p))) out = p;
    CoTaskMemFree(p);
    return out;
}

// Earlier builds kept their files next to the .exe — move them into the hidden folder.
// MoveFileEx works for folders only within one drive; otherwise the old files stay where they are.
void MigrateOldFiles() {
    static const wchar_t* ITEMS[] = { L"config.txt", L"imported_versions", L"backups" };
    for (const wchar_t* item : ITEMS) {
        std::wstring from = Globals::EXE_DIR + L"\\" + item;
        std::wstring to = Globals::DATA_DIR + L"\\" + item;
        if (GetFileAttributesW(from.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        if (GetFileAttributesW(to.c_str()) != INVALID_FILE_ATTRIBUTES) continue;
        MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
    }
}

} // namespace

bool cApp::OnInit() {
    // Locate the .exe itself, not the current working directory (differs when started via a shortcut).
    wchar_t exe[MAX_PATH * 4]{};
    GetModuleFileNameW(nullptr, exe, (DWORD)std::size(exe));
    std::wstring dir(exe);
    size_t slash = dir.find_last_of(L"\\/");
    Globals::EXE_DIR = (slash == std::wstring::npos) ? L"." : dir.substr(0, slash);

    // Everything else goes to %LOCALAPPDATA%\MinecraftInstaller, marked hidden + system
    // so Explorer does not show it even with "show hidden files" enabled.
    std::wstring base = KnownFolder(FOLDERID_LocalAppData);
    Globals::DATA_DIR = base.empty() ? Globals::EXE_DIR : base + L"\\" + Globals::DATA_DIR_NAME;
    CreateDirectoryW(Globals::DATA_DIR.c_str(), nullptr);
    if (!base.empty()) {
        DWORD attr = GetFileAttributesW(Globals::DATA_DIR.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES)
            SetFileAttributesW(Globals::DATA_DIR.c_str(), attr | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        MigrateOldFiles();
    }

    wxInitAllImageHandlers();

    CMAIN_INSTANCE = new cMain();
    CMAIN_INSTANCE->Show();
    return true;
}
