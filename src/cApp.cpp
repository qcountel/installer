#include "pch.h"
#include "cApp.h"

wxIMPLEMENT_APP(cApp);

bool cApp::OnInit() {
    // Config and imported_versions live next to the .exe,
    // not in the current working directory (which differs when started via a shortcut).
    wchar_t exe[MAX_PATH * 4]{};
    GetModuleFileNameW(nullptr, exe, (DWORD)std::size(exe));
    std::wstring dir(exe);
    size_t slash = dir.find_last_of(L"\\/");
    Globals::EXE_DIR = (slash == std::wstring::npos) ? L"." : dir.substr(0, slash);

    wxInitAllImageHandlers();

    CMAIN_INSTANCE = new cMain();
    CMAIN_INSTANCE->Show();
    return true;
}
