#include "pch.h"
#include "config.h"

// Config is stored as UTF-8 text in Globals::DATA_DIR so Cyrillic paths survive.

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

Config::Config() {
    this->path = Globals::DATA_DIR + L"\\config.txt";
}

bool Config::load() {
    std::ifstream f(this->path, std::ios::binary);
    if (!f.is_open()) return false;

    std::string line;
    bool first = true;
    while (std::getline(f, line)) {
        if (first) { // strip UTF-8 BOM
            if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
                line.erase(0, 3);
            first = false;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        this->apply(Utf8ToWide(line.substr(0, eq)), Utf8ToWide(line.substr(eq + 1)));
    }
    return true;
}

bool Config::save() const {
    std::ofstream f(this->path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f << "# Minecraft Installer config (UTF-8)\n";
    f << "deleteAppx=" << (Globals::DELETE_APPX ? "true" : "false") << "\n";
    f << "keyPatch=" << (Globals::APPLY_KEYPATCH ? "true" : "false") << "\n";
    f << "gdrive=" << (Globals::GDRIVE_ENABLED ? "true" : "false") << "\n";
    f << "gdriveMode=" << (Globals::GDRIVE_MODE == Globals::GDriveMode::Cert ? "cert" : "dev") << "\n";
    f << "version=" << WideToUtf8(Globals::SELECTED_VERSION) << "\n";
    f << "versionsUrl=" << WideToUtf8(Globals::VERSIONS_URL) << "\n";
    return true;
}

void Config::apply(const std::wstring& key, const std::wstring& val) {
    auto yes = [&] { return val == L"true" || val == L"1"; };
    if (key == L"deleteAppx")        Globals::DELETE_APPX = yes();
    else if (key == L"keyPatch")     Globals::APPLY_KEYPATCH = yes();
    else if (key == L"gdrive")       Globals::GDRIVE_ENABLED = yes();
    else if (key == L"gdriveMode")   Globals::GDRIVE_MODE = (val == L"cert") ? Globals::GDriveMode::Cert : Globals::GDriveMode::Dev;
    else if (key == L"version")      { if (!val.empty()) Globals::SELECTED_VERSION = val; }
    else if (key == L"versionsUrl")  { if (!val.empty()) Globals::VERSIONS_URL = val; }
}
