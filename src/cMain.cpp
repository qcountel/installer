#include "pch.h"
#include "cMain.h"
#include "installer.h"
#include "appx.h"
#include "keypatch.h"
#include "net.h"
#include "wu.h"
#include "theme.h"

#include <algorithm>
#include <filesystem>
#include <map>

enum {
    ID_TAB_INSTALL = 101,
    ID_TAB_SETTINGS,
    ID_MAIN,
    ID_VERSION,
    ID_OPEN_FOLDER,
    ID_SAVE,
    ID_VERSION_DEFAULT = 1900,
    ID_VERSION_BASE = 2000,          // + index in cMain::versions
    ID_VERSION_LAST = ID_VERSION_BASE + 4999,
};

static const int TAB_H = 42;

static const wchar_t* EMBLEM_TITLE = L"MCBE";
static const wchar_t* STEP_NAMES[cMain::STEP_COUNT] = {
    L"Загрузка", L"Распаковка", L"Патч Xbox Live", L"Регистрация" };

// Overall progress share of each step: download 0-55, unpack 55-80, patch 80-83, register 83-100.
static const int STEP_FROM[cMain::STEP_COUNT] = { 0, 55, 80, 83 };
static const int STEP_TO[cMain::STEP_COUNT]   = { 55, 80, 83, 100 };

wxBEGIN_EVENT_TABLE(cMain, wxFrame)
EVT_BUTTON(ID_MAIN, cMain::OnMainButton)
EVT_BUTTON(ID_VERSION, cMain::OnVersionButton)
EVT_BUTTON(ID_SAVE, cMain::OnSave)
EVT_CLOSE(cMain::OnClose)
wxEND_EVENT_TABLE();

static wxStaticText* MakeLabel(wxWindow* parent, const wxString& text, wxColour bg, bool dim = false) {
    wxStaticText* s = new wxStaticText(parent, wxID_ANY, text);
    s->SetForegroundColour(dim ? Theme::FG_DIM : Theme::FG);
    s->SetBackgroundColour(bg);
    s->SetFont(Theme::Font(dim ? 8 : 10));
    return s;
}

static wxCheckBox* MakeCheck(wxWindow* parent, const wxString& text) {
    wxCheckBox* c = new wxCheckBox(parent, wxID_ANY, text);
    c->SetForegroundColour(Theme::FG);
    c->SetBackgroundColour(Theme::CARD);
    c->SetFont(Theme::Font(9));
    return c;
}

static wxRadioButton* MakeRadio(wxWindow* parent, const wxString& text, bool first) {
    wxRadioButton* r = new wxRadioButton(parent, wxID_ANY, text, wxDefaultPosition, wxDefaultSize,
                                         first ? wxRB_GROUP : 0);
    r->SetForegroundColour(Theme::FG);
    r->SetBackgroundColour(Theme::CARD);
    r->SetFont(Theme::Font(9));
    return r;
}

static wxPanel* MakeCard(wxWindow* parent, const wxString& title, wxBoxSizer*& outSizer) {
    wxPanel* card = new wxPanel(parent, wxID_ANY);
    card->SetBackgroundColour(Theme::CARD);
    outSizer = new wxBoxSizer(wxVERTICAL);
    outSizer->Add(MakeLabel(card, title, Theme::CARD), 0, wxALL, 12);
    card->SetSizer(outSizer);
    return card;
}

cMain::cMain()
    : wxFrame(nullptr, wxID_ANY, L"Minecraft Installer", wxDefaultPosition, wxSize(420, 720),
              wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN) {

    Theme::EnsurePixelFont();

    if (!this->cfg.load()) this->cfg.save();

    this->SetIcon(wxIcon(icon_xpm));
    Theme::ApplyDarkTitleBar((HWND)this->GetHandle());
    this->SetBackgroundColour(Theme::BG);

    this->selected = { Globals::DEFAULT_VERSION, Globals::DEFAULT_UPDATE_ID };
    this->versions = { this->selected };
    this->steps.fill(StepState::Pending);

    // ---- Tabs ----
    this->tab_Install = new FlatButton(this, ID_TAB_INSTALL, L"УСТАНОВКА");
    this->tab_Install->SetButtonStyle(FlatButton::STYLE_TAB);
    this->tab_Install->SetFont(Theme::Font(10));
    this->tab_Install->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { this->ShowPage(false); });

    this->tab_Settings = new FlatButton(this, ID_TAB_SETTINGS, L"НАСТРОЙКИ");
    this->tab_Settings->SetButtonStyle(FlatButton::STYLE_TAB);
    this->tab_Settings->SetFont(Theme::Font(10));
    this->tab_Settings->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { this->ShowPage(true); });

    // =================== INSTALL PAGE ===================
    this->pageInstall = new wxPanel(this, wxID_ANY);
    this->pageInstall->SetBackgroundColour(Theme::BG);
    this->pageInstall->SetBackgroundStyle(wxBG_STYLE_PAINT);
    this->pageInstall->Bind(wxEVT_PAINT, &cMain::OnInstallPagePaint, this);

    this->btn_Version = new FlatButton(this->pageInstall, ID_VERSION, L"", wxDefaultPosition, wxSize(300, 40));
    this->btn_Version->SetFont(Theme::Font(9));

    this->btn_Main = new FlatButton(this->pageInstall, ID_MAIN, L"СКАЧАТЬ", wxDefaultPosition, wxSize(300, 76));
    this->btn_Main->SetFont(Theme::Font(18));

    this->lbl_Status = new wxStaticText(this->pageInstall, wxID_ANY, L"",
        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
    this->lbl_Status->SetForegroundColour(Theme::FG_DIM);
    this->lbl_Status->SetBackgroundColour(Theme::BG);
    this->lbl_Status->SetFont(Theme::Font(8));

    this->pageInstall->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { this->layoutInstallPage(); e.Skip(); });

    // Popup menu items (version list) are routed to the frame.
    this->Bind(wxEVT_MENU, &cMain::OnVersionPicked, this, ID_VERSION_DEFAULT);
    this->Bind(wxEVT_MENU, &cMain::OnVersionPicked, this, ID_VERSION_BASE, ID_VERSION_LAST);

    // =================== SETTINGS PAGE ===================
    this->pageSettings = new wxPanel(this, wxID_ANY);
    this->pageSettings->SetBackgroundColour(Theme::BG);
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* s = nullptr;
    wxPanel* gd = MakeCard(this->pageSettings, L"СБОРКА 1.16.100.4", s);
    this->chk_GDrive = MakeCheck(gd, L"Скачивание с Google Дисков");
    s->Add(this->chk_GDrive, 0, wxLEFT | wxRIGHT, 12);
    this->rb_GDriveDev = MakeRadio(gd, L"Через режим разработчика (+ патч Xbox)", true);
    this->rb_GDriveCert = MakeRadio(gd, L"Через сертификат (без Xbox Live)", false);
    s->Add(this->rb_GDriveDev, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    s->Add(this->rb_GDriveCert, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    s->Add(MakeLabel(gd, L"Сборка со встроенным ресурспаком. Выключено —\nофициальная версия с серверов Microsoft", Theme::CARD, true), 0, wxALL, 12);
    this->chk_GDrive->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        bool on = this->chk_GDrive->GetValue();
        this->rb_GDriveDev->Enable(on);
        this->rb_GDriveCert->Enable(on);
    });
    root->Add(gd, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);

    wxPanel* xbox = MakeCard(this->pageSettings, L"XBOX LIVE", s);
    this->chk_KeyPatch = MakeCheck(xbox, L"Патчить ключ Xbox Live (KeyPatcher)");
    s->Add(this->chk_KeyPatch, 0, wxLEFT | wxRIGHT, 12);
    s->Add(MakeLabel(xbox, L"Старые версии не входят в Xbox: Mojang сменила ключ.\nПатч заменяет его в Minecraft.Windows.exe", Theme::CARD, true), 0, wxALL, 12);
    root->Add(xbox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);

    wxPanel* files = MakeCard(this->pageSettings, L"ФАЙЛЫ", s);
    this->chk_DeleteAppx = MakeCheck(files, L"Удалять .appx после распаковки");
    s->Add(this->chk_DeleteAppx, 0, wxLEFT | wxRIGHT, 12);
    s->Add(MakeLabel(files, L"Скрытая папка %LOCALAPPDATA%\\MinecraftInstaller", Theme::CARD, true), 0, wxALL, 12);
    this->btn_OpenFolder = new FlatButton(files, ID_OPEN_FOLDER, L"ОТКРЫТЬ ПАПКУ", wxDefaultPosition, wxSize(-1, 36));
    this->btn_OpenFolder->SetFont(Theme::Font(9));
    this->btn_OpenFolder->Bind(wxEVT_BUTTON, [](wxCommandEvent&) { Installer::OpenFolder(Globals::DATA_DIR); });
    s->Add(this->btn_OpenFolder, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    root->Add(files, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);

    wxPanel* list = MakeCard(this->pageSettings, L"СПИСОК ВЕРСИЙ", s);
    this->txt_VersionsUrl = new wxTextCtrl(list, wxID_ANY, L"", wxDefaultPosition, wxSize(-1, 30));
    this->txt_VersionsUrl->SetBackgroundColour(Theme::INPUT_BG);
    this->txt_VersionsUrl->SetForegroundColour(Theme::FG);
    this->txt_VersionsUrl->SetFont(Theme::Font(9));
    s->Add(this->txt_VersionsUrl, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);
    s->Add(MakeLabel(list, L"База версий MCLauncher (применится после перезапуска)", Theme::CARD, true), 0, wxALL, 12);
    root->Add(list, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);

    root->AddStretchSpacer(1);

    this->btn_Save = new FlatButton(this->pageSettings, ID_SAVE, L"СОХРАНИТЬ", wxDefaultPosition, wxSize(-1, 52));
    this->btn_Save->SetFont(Theme::Font(11));
    root->Add(this->btn_Save, 0, wxEXPAND | wxALL, 14);
    this->pageSettings->SetSizer(root);

    this->fillSettings();

    this->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { this->layoutTabs(); e.Skip(); });
    this->SetMinSize(wxSize(420, 700));
    this->ShowPage(false);

    this->selectVersion(this->selected);
    this->setStatus(L"Загружаю список версий...");

    // The online version list is loaded in the background; 1.16.100.4 works without it.
    this->listLoader = std::thread([this] {
        std::wstring st;
        std::vector<VersionInfo> list = Versions::Load(st);
        this->CallAfter([this, list, st] {
            this->versions = list;
            if (!this->busy.load()) {
                this->setStatus(st + L" (" + std::to_wstring(list.size()) + L")");
                this->refreshMainButton();
            }
        });
    });
}

cMain::~cMain() {
    this->cancel.store(true);
    if (this->worker.joinable()) this->worker.join();
    if (this->listLoader.joinable()) this->listLoader.join();
}

void cMain::OnClose(wxCloseEvent& evt) {
    if (this->busy.load() && evt.CanVeto()) {
        int r = wxMessageBox(L"Установка ещё идёт. Прервать её и выйти?", L"Установка идёт",
                             wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this);
        if (r != wxYES) { evt.Veto(); return; }
        // Download and unpack stop at the next chunk; registration is finished by Windows itself.
        this->cancel.store(true);
    }
    evt.Skip();
}

// --------------------------------------------------------------------------- layout
void cMain::layoutTabs() {
    wxSize sz = this->GetClientSize();
    int w = sz.GetWidth(), h = sz.GetHeight();
    int halfW = w / 2;
    this->tab_Install->SetSize(0, 0, halfW, TAB_H);
    this->tab_Settings->SetSize(halfW, 0, w - halfW, TAB_H);
    this->pageInstall->SetSize(0, TAB_H, w, h - TAB_H);
    this->pageSettings->SetSize(0, TAB_H, w, h - TAB_H);
}

void cMain::layoutInstallPage() {
    wxSize sz = this->pageInstall->GetClientSize();
    int w = sz.GetWidth(), h = sz.GetHeight();
    wxSize bs = this->btn_Main->GetSize();
    int bx = (w - bs.GetWidth()) / 2;
    int by = h - bs.GetHeight() - 100;
    wxSize vs = this->btn_Version->GetSize();
    this->btn_Version->SetPosition(wxPoint((w - vs.GetWidth()) / 2, by - vs.GetHeight() - 12));
    this->btn_Main->SetPosition(wxPoint(bx, by));
    this->lbl_Status->SetSize(16, by + bs.GetHeight() + 12, w - 32, 84);
    this->pageInstall->Refresh();
}

void cMain::ShowPage(bool settings) {
    this->tab_Install->SetSelected(!settings);
    this->tab_Settings->SetSelected(settings);
    this->pageInstall->Show(!settings);
    this->pageSettings->Show(settings);
    this->layoutTabs();
    if (settings) this->pageSettings->Layout();
}

void cMain::setStatus(const wxString& msg) {
    this->lbl_Status->SetLabel(msg);
    this->lbl_Status->Wrap(this->lbl_Status->GetSize().GetWidth());
    this->pageInstall->Refresh();
}

// --------------------------------------------------------------------------- versions
void cMain::selectVersion(const VersionInfo& v) {
    this->selected = v;
    bool isDefault = v.name == Globals::DEFAULT_VERSION;
    this->btn_Version->SetCaption(L"ВЕРСИЯ: " + v.name + (isDefault ? L" ★" : L"") +
                                  (this->sourceFor(v) != Source::Official ? L" · ДИСК" : L""));
    this->steps.fill(StepState::Pending);
    this->progress = 0;
    this->stepProgress = 0;
    this->stepDetail.clear();
    this->refreshMainButton();
}

cMain::Source cMain::sourceFor(const VersionInfo& v) const {
    if (!Globals::GDRIVE_ENABLED || v.name != Globals::DEFAULT_VERSION) return Source::Official;
    return Globals::GDRIVE_MODE == Globals::GDriveMode::Cert ? Source::GDriveCert : Source::GDriveDev;
}

namespace {
std::wstring SignedAppx(const std::wstring& version) { return Versions::Dir(version, Versions::GDRIVE_SIGNED_SUFFIX) + L"\\Minecraft.appx"; }
std::wstring SignedCer(const std::wstring& version)  { return Versions::Dir(version, Versions::GDRIVE_SIGNED_SUFFIX) + L"\\certificate.cer"; }
bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
bool SignedReady(const std::wstring& version) {
    return Exists(Versions::ExtractedMarker(version, Versions::GDRIVE_SIGNED_SUFFIX)) &&
           Exists(SignedAppx(version)) && Exists(SignedCer(version));
}
// Version of the package in the Drive build (1.16.100.4 -> 1.16.10004.0).
const wchar_t* GDRIVE_PACKAGE_VERSION = L"1.16.10004.0";
}

void cMain::refreshMainButton() {
    const Source src = this->sourceFor(this->selected);
    const Installer::PackageState pkg = Installer::Current();
    bool ready;
    if (src == Source::GDriveCert) {
        // The signed build lives in WindowsApps; a Store copy of 1.16.100.4 is no longer possible.
        this->playReady = pkg.found && !pkg.devMode && pkg.version == GDRIVE_PACKAGE_VERSION;
        ready = SignedReady(this->selected.name);
    } else {
        const std::wstring suffix = src == Source::GDriveDev ? Versions::GDRIVE_SUFFIX : L"";
        std::wstring dir = Versions::Dir(this->selected.name, suffix);
        this->playReady = pkg.found && pkg.devMode && _wcsicmp(pkg.location.c_str(), dir.c_str()) == 0;
        ready = Versions::IsExtracted(this->selected.name, suffix);
    }
    if (this->playReady) this->btn_Main->SetCaption(L"ИГРАТЬ");
    else if (ready)      this->btn_Main->SetCaption(L"УСТАНОВИТЬ");
    else                 this->btn_Main->SetCaption(L"СКАЧАТЬ");
    this->pageInstall->Refresh();
}

void cMain::OnVersionButton(wxCommandEvent&) {
    if (this->busy.load()) return;

    auto label = [this](const VersionInfo& v) {
        wxString s = v.name;
        if (v.name == Globals::DEFAULT_VERSION) s += L"  (рекомендуется)";
        if (Versions::IsExtracted(v.name)) s += L"  [скачана]";
        return s;
    };

    wxMenu menu;
    wxMenuItem* def = menu.AppendCheckItem(ID_VERSION_DEFAULT, label({ Globals::DEFAULT_VERSION, L"" }));
    def->Check(this->selected.name == Globals::DEFAULT_VERSION);
    menu.AppendSeparator();

    // Group by "major.minor", keeping the newest-first order.
    std::vector<std::pair<std::wstring, wxMenu*>> groups;
    for (size_t i = 0; i < this->versions.size() && i < 5000; ++i) {
        const VersionInfo& v = this->versions[i];
        std::wstring major = Versions::Major(v.name);
        if (groups.empty() || groups.back().first != major) groups.push_back({ major, new wxMenu() });
        wxMenuItem* item = groups.back().second->AppendCheckItem(ID_VERSION_BASE + (int)i, label(v));
        item->Check(v.name == this->selected.name);
    }
    for (auto& [major, sub] : groups) menu.AppendSubMenu(sub, major);

    wxPoint pos = this->btn_Version->GetPosition();
    this->pageInstall->PopupMenu(&menu, pos.x, pos.y + this->btn_Version->GetSize().GetHeight());
}

void cMain::OnVersionPicked(wxCommandEvent& evt) {
    if (this->busy.load()) return;
    int id = evt.GetId();
    if (id == ID_VERSION_DEFAULT) {
        this->selectVersion({ Globals::DEFAULT_VERSION, Globals::DEFAULT_UPDATE_ID });
    } else {
        size_t idx = (size_t)(id - ID_VERSION_BASE);
        if (idx >= this->versions.size()) return;
        this->selectVersion(this->versions[idx]);
    }
    if (this->selected.name != Globals::DEFAULT_VERSION)
        this->setStatus(L"Выбрана " + this->selected.name + L". Рекомендуемая версия — " +
                        std::wstring(Globals::DEFAULT_VERSION));
    else
        this->setStatus(L"Выбрана " + this->selected.name);
}

// --------------------------------------------------------------------------- painting
void cMain::OnInstallPagePaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this->pageInstall);
    dc.SetBackground(wxBrush(Theme::BG));
    dc.Clear();

    wxSize sz = this->pageInstall->GetClientSize();
    int w = sz.GetWidth();

    // ---- Emblem ----
    const int box = 92;
    int bx = (w - box) / 2;
    int by = 18;
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(Theme::OUTLINE));
    dc.DrawRectangle(bx - 4, by - 4, box + 8, box + 8);
    dc.SetBrush(wxBrush(Theme::CARD));
    dc.DrawRectangle(bx, by, box, box);
    dc.SetBrush(wxBrush(wxColour(58, 58, 58)));
    dc.DrawRectangle(bx, by, box, 4);
    dc.DrawRectangle(bx, by, 4, box);
    dc.SetBrush(wxBrush(wxColour(20, 20, 20)));
    dc.DrawRectangle(bx, by + box - 4, box, 4);
    dc.DrawRectangle(bx + box - 4, by, 4, box);

    wxCoord tw, th;
    dc.SetFont(Theme::Font(11));
    dc.GetTextExtent(EMBLEM_TITLE, &tw, &th);
    dc.SetTextForeground(Theme::SHADOW);
    dc.DrawText(EMBLEM_TITLE, bx + (box - tw) / 2 + 2, by + box / 2 - th);
    dc.SetTextForeground(Theme::FG);
    dc.DrawText(EMBLEM_TITLE, bx + (box - tw) / 2, by + box / 2 - th - 2);
    wxString sub = this->selected.name;
    dc.SetFont(Theme::Font(7));
    dc.GetTextExtent(sub, &tw, &th);
    dc.SetTextForeground(Theme::FG_DIM);
    dc.DrawText(sub, bx + (box - tw) / 2, by + box / 2 + 4);

    // ---- Current step heading ----
    int current = -1;
    for (int k = 0; k < STEP_COUNT; ++k)
        if (this->steps[k] == StepState::Running || this->steps[k] == StepState::Failed) { current = k; break; }
    bool allDone = this->steps[STEP_REGISTER] == StepState::Done;

    wxString heading;
    if (allDone)           heading = L"ВСЁ ГОТОВО";
    else if (current >= 0) heading = wxString::Format(L"ШАГ %d ИЗ %d: %s", current + 1, (int)STEP_COUNT,
                                                      wxString(STEP_NAMES[current]).Upper());
    else                   heading = wxString::Format(L"%d ШАГА ДО ИГРЫ", (int)STEP_COUNT);
    int top = by + box + 18;
    dc.SetFont(Theme::Font(10));
    dc.GetTextExtent(heading, &tw, &th);
    dc.SetTextForeground(Theme::FG);
    dc.DrawText(heading, (w - tw) / 2, top);

    // ---- Step list ----
    auto mark = [](StepState st) -> wxString {
        switch (st) {
        case StepState::Running: return L"[..]";
        case StepState::Done:    return L"[OK]";
        case StepState::Failed:  return L"[!!]";
        default:                 return L"[  ]";
        }
    };
    int sx = 44, sy = top + 30;
    dc.SetFont(Theme::Font(9));
    for (int k = 0; k < STEP_COUNT; ++k) {
        bool lit = this->steps[k] == StepState::Done || this->steps[k] == StepState::Running;
        dc.SetTextForeground(lit ? Theme::FG : Theme::FG_DIM);
        dc.DrawText(mark(this->steps[k]) + wxString::Format(L" %d. ", k + 1) + STEP_NAMES[k], sx, sy + k * 22);
    }

    // ---- Progress bars ----
    auto drawBar = [&](int x, int y, int bw, int pct, const wxString& left, const wxString& right) {
        dc.SetFont(Theme::Font(8));
        dc.SetTextForeground(Theme::FG_DIM);
        dc.DrawText(left, x, y - 16);
        wxCoord rw, rh;
        dc.GetTextExtent(right, &rw, &rh);
        dc.DrawText(right, x + bw - rw, y - 16);
        const int bh = 12;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(Theme::OUTLINE));
        dc.DrawRectangle(x - 2, y - 2, bw + 4, bh + 4);
        dc.SetBrush(wxBrush(Theme::INPUT_BG));
        dc.DrawRectangle(x, y, bw, bh);
        int fill = bw * std::clamp(pct, 0, 100) / 100;
        if (fill > 0) {
            dc.SetBrush(wxBrush(Theme::STONE));
            dc.DrawRectangle(x, y, fill, bh);
            dc.SetBrush(wxBrush(Theme::STONE_LIGHT));
            dc.DrawRectangle(x, y, fill, 3);
            dc.SetBrush(wxBrush(Theme::STONE_DARK));
            dc.DrawRectangle(x, y + bh - 2, fill, 2);
        }
    };

    int px = 44, pw = w - 88;
    int stepBarY = sy + STEP_COUNT * 22 + 26;
    wxString stepLabel = current >= 0 ? wxString(STEP_NAMES[current]) : (allDone ? wxString(L"Готово") : wxString(L"Текущий шаг"));
    drawBar(px, stepBarY, pw, this->stepProgress, stepLabel, wxString::Format(L"%d%%", this->stepProgress));

    dc.SetFont(Theme::Font(7));
    dc.SetTextForeground(Theme::FG_DIM);
    dc.DrawText(this->stepDetail, px, stepBarY + 18);

    int totalBarY = stepBarY + 56;
    drawBar(px, totalBarY, pw, this->progress, L"Всего", wxString::Format(L"%d%%", this->progress));
}

// --------------------------------------------------------------------------- thread -> UI
void cMain::postStatus(const std::wstring& msg) {
    this->CallAfter([this, msg] { this->setStatus(msg); });
}
void cMain::postStep(Step step, StepState st) {
    this->CallAfter([this, step, st] {
        this->steps[step] = st;
        this->pageInstall->Refresh();
    });
}
void cMain::postProgress(int overall, int step) {
    this->CallAfter([this, overall, step] {
        this->progress = overall;
        this->stepProgress = step;
        this->pageInstall->Refresh();
    });
}
void cMain::postDetail(const std::wstring& text) {
    this->CallAfter([this, text] {
        this->stepDetail = text;
        this->pageInstall->Refresh();
    });
}

// --------------------------------------------------------------------------- install
namespace {

std::wstring FormatSize(uint64_t bytes) {
    wchar_t buf[32];
    if (bytes >= 1073741824ull) swprintf_s(buf, L"%.2f ГБ", bytes / 1073741824.0);
    else                        swprintf_s(buf, L"%.0f МБ", bytes / 1048576.0);
    return buf;
}

std::wstring FormatSpeed(double bytesPerSec) {
    wchar_t buf[32];
    if (bytesPerSec >= 1048576.0) swprintf_s(buf, L"%.1f МБ/с", bytesPerSec / 1048576.0);
    else                          swprintf_s(buf, L"%.0f КБ/с", bytesPerSec / 1024.0);
    return buf;
}

std::wstring FormatEta(double seconds) {
    if (seconds < 0 || seconds > 360000) return L"";
    wchar_t buf[48];
    int s = (int)seconds;
    if (s < 60)        swprintf_s(buf, L"осталось ~%d с", s);
    else if (s < 3600) swprintf_s(buf, L"осталось ~%d мин %02d с", s / 60, s % 60);
    else               swprintf_s(buf, L"осталось ~%d ч %02d мин", s / 3600, (s % 3600) / 60);
    return buf;
}

int Scale(int step, uint64_t done, uint64_t total) {
    if (!total) return STEP_FROM[step];
    uint64_t d = (std::min)(done, total);
    return STEP_FROM[step] + (int)((STEP_TO[step] - STEP_FROM[step]) * d / total);
}

int Percent(uint64_t done, uint64_t total) {
    return total ? (int)((std::min)(done, total) * 100 / total) : 0;
}

} // namespace

void cMain::OnMainButton(wxCommandEvent&) {
    if (this->busy.load()) return;

    if (this->playReady) {
        if (!Installer::Launch())
            this->setStatus(L"Не удалось запустить игру");
        return;
    }
    this->startInstall(this->sourceFor(this->selected));
}

void cMain::startInstall(Source src) {
    if (this->busy.load()) return;

    // Registering an unsigned folder requires Developer Mode — check before downloading 300 MB.
    // The signed Drive build (certificate mode) is installed without it.
    if (src != Source::GDriveCert && !Installer::IsDeveloperModeEnabled()) {
        wxMessageBox(L"Для установки нужен режим разработчика Windows.\n\n"
                     L"Сейчас откроются Параметры → Для разработчиков: включите «Режим разработчика» "
                     L"и нажмите кнопку ещё раз.", L"Нужен режим разработчика", wxOK | wxICON_INFORMATION, this);
        Installer::OpenDeveloperSettings();
        this->setStatus(L"Включите режим разработчика и нажмите ещё раз");
        return;
    }
    if (Installer::IsGameRunning()) {
        this->setStatus(L"Minecraft запущен — закройте игру и нажмите ещё раз");
        return;
    }

    if (this->worker.joinable()) this->worker.join();

    this->busy.store(true);
    this->cancel.store(false);
    this->steps.fill(StepState::Pending);
    this->progress = 0;
    this->stepProgress = 0;
    this->stepDetail.clear();
    this->btn_Main->Enable(false);
    this->btn_Main->SetCaption(L"...");
    this->btn_Version->Enable(false);
    this->tab_Settings->Enable(false);
    this->pageInstall->Refresh();

    this->worker = std::thread(&cMain::InstallWorker, this, this->selected, src);
}

// Step 1 + 2: Microsoft CDN -> imported_versions\Minecraft_X_x64.appx -> imported_versions\Minecraft_X_x64\.
bool cMain::DownloadAndExtract(const VersionInfo& v, std::wstring& status) {
    const std::wstring root = Versions::Root();
    const std::wstring dir = Versions::Dir(v.name);
    const std::wstring appx = root + L"\\" + Versions::FolderName(v.name) + L".appx";
    const bool isDefault = v.name == Globals::DEFAULT_VERSION;
    CreateDirectoryW(root.c_str(), nullptr);

    // ---- Step 1: download ----
    this->postStep(STEP_DOWNLOAD, StepState::Running);
    bool haveAppx = GetFileAttributesW(appx.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (haveAppx && isDefault) {
        this->postStatus(L"Проверяю ранее скачанный пакет...");
        haveAppx = Net::Sha256File(appx) == Globals::DEFAULT_APPX_SHA256;
        if (!haveAppx) DeleteFileW(appx.c_str());
    }

    if (!haveAppx) {
        ULARGE_INTEGER freeBytes{};
        if (GetDiskFreeSpaceExW(root.c_str(), &freeBytes, nullptr, nullptr) &&
            freeBytes.QuadPart < 1500ull * 1024 * 1024) {
            status = L"Мало места на диске: нужно около 1.5 ГБ, свободно " + FormatSize(freeBytes.QuadPart);
            this->postStep(STEP_DOWNLOAD, StepState::Failed);
            return false;
        }

        this->postStatus(L"Запрашиваю ссылку у Microsoft (как MCLauncher)...");
        std::wstring url;
        if (!WU::ResolveDownloadUrl(v.updateId, url, status)) {
            this->postStep(STEP_DOWNLOAD, StepState::Failed);
            return false;
        }

        this->postStatus(L"Скачиваю Minecraft " + v.name + L" с серверов Microsoft...");
        DWORD startTick = GetTickCount(), lastTick = 0;
        bool ok = Net::DownloadFile(url, appx, [&](uint64_t done, uint64_t total) {
            DWORD now = GetTickCount();
            if (now - lastTick < 200 && done < total) return;   // throttle UI updates
            lastTick = now;
            double elapsed = (now - startTick) / 1000.0;
            double speed = elapsed > 0.5 ? done / elapsed : 0.0;
            std::wstring detail = FormatSize(done) + (total ? L" из " + FormatSize(total) : L"");
            if (speed > 0 && total > done) {
                detail += L" · " + FormatSpeed(speed);
                std::wstring eta = FormatEta((total - done) / speed);
                if (!eta.empty()) detail += L" · " + eta;
            }
            this->postDetail(detail);
            this->postProgress(Scale(STEP_DOWNLOAD, done, total), Percent(done, total));
        }, &this->cancel, status);
        if (!ok) {
            status = L"Загрузка не удалась: " + status;
            this->postStep(STEP_DOWNLOAD, StepState::Failed);
            return false;
        }

        if (isDefault) {
            this->postStatus(L"Проверяю контрольную сумму...");
            if (Net::Sha256File(appx) != Globals::DEFAULT_APPX_SHA256) {
                DeleteFileW(appx.c_str());
                status = L"Скачанный пакет повреждён (не совпала SHA-256). Попробуйте ещё раз";
                this->postStep(STEP_DOWNLOAD, StepState::Failed);
                return false;
            }
        }
    }
    this->postStep(STEP_DOWNLOAD, StepState::Done);
    this->postProgress(STEP_TO[STEP_DOWNLOAD], 100);

    // ---- Step 2: unpack ----
    this->postStep(STEP_EXTRACT, StepState::Running);
    this->postStatus(L"Распаковываю в " + dir);
    this->postDetail(L"");
    std::error_code ec;
    std::filesystem::remove_all(L"\\\\?\\" + dir, ec);   // leftovers of an interrupted unpack
    bool ok = Appx::Extract(appx, dir, [&](uint64_t done, uint64_t total) {
        this->postDetail(FormatSize(done) + L" из " + FormatSize(total));
        this->postProgress(Scale(STEP_EXTRACT, done, total), Percent(done, total));
    }, &this->cancel, status);
    if (!ok) {
        status = L"Распаковка не удалась: " + status;
        this->postStep(STEP_EXTRACT, StepState::Failed);
        return false;
    }
    std::ofstream(Versions::ExtractedMarker(v.name)) << "ok";
    if (Globals::DELETE_APPX) DeleteFileW(appx.c_str());
    this->postStep(STEP_EXTRACT, StepState::Done);
    this->postProgress(STEP_TO[STEP_EXTRACT], 100);
    return true;
}

// Google Drive build: <root>\Minecraft_X_x64_gdrive.zip -> (.appx + .cer) -> unpacked folder or signed files.
bool cMain::DownloadGDrive(const VersionInfo& v, Source src, std::wstring& status, bool& driveFailed) {
    driveFailed = false;
    const std::wstring root = Versions::Root();
    const std::wstring zip = root + L"\\" + Versions::FolderName(v.name, Versions::GDRIVE_SUFFIX) + L".zip";
    CreateDirectoryW(root.c_str(), nullptr);

    // ---- Step 1: download from Google Drive ----
    this->postStep(STEP_DOWNLOAD, StepState::Running);
    ULARGE_INTEGER freeBytes{};
    if (GetDiskFreeSpaceExW(root.c_str(), &freeBytes, nullptr, nullptr) &&
        freeBytes.QuadPart < 1500ull * 1024 * 1024) {
        status = L"Мало места на диске: нужно около 1.5 ГБ, свободно " + FormatSize(freeBytes.QuadPart);
        this->postStep(STEP_DOWNLOAD, StepState::Failed);
        return false;
    }

    this->postStatus(L"Скачиваю сборку " + v.name + L" с Google Диска...");
    DWORD startTick = GetTickCount(), lastTick = 0;
    bool ok = Net::DownloadFile(Globals::GDRIVE_URL, zip, [&](uint64_t done, uint64_t total) {
        DWORD now = GetTickCount();
        if (now - lastTick < 200 && done < total) return;
        lastTick = now;
        double elapsed = (now - startTick) / 1000.0;
        double speed = elapsed > 0.5 ? done / elapsed : 0.0;
        std::wstring detail = FormatSize(done) + (total ? L" из " + FormatSize(total) : L"");
        if (speed > 0 && total > done) {
            detail += L" · " + FormatSpeed(speed);
            std::wstring eta = FormatEta((total - done) / speed);
            if (!eta.empty()) detail += L" · " + eta;
        }
        this->postDetail(detail);
        this->postProgress(Scale(STEP_DOWNLOAD, done, total), Percent(done, total));
    }, &this->cancel, status);
    if (!ok) {
        driveFailed = !this->cancel.load();
        status = L"загрузка не удалась: " + status;
        this->postStep(STEP_DOWNLOAD, StepState::Failed);
        return false;
    }

    // Drive answers with an HTML page (quota exceeded, file removed, access denied) instead of the zip.
    char sig[4]{};
    {
        std::ifstream f(zip, std::ios::binary);
        f.read(sig, 4);
    }
    if (!(sig[0] == 'P' && sig[1] == 'K' && sig[2] == 3 && sig[3] == 4)) {
        DeleteFileW(zip.c_str());
        driveFailed = true;
        status = L"Google Диск вернул не архив: превышен лимит скачиваний, файл удалён или закрыт доступ";
        this->postStep(STEP_DOWNLOAD, StepState::Failed);
        return false;
    }
    this->postStep(STEP_DOWNLOAD, StepState::Done);
    this->postProgress(STEP_TO[STEP_DOWNLOAD], 100);

    // ---- Step 2: unpack ----
    this->postStep(STEP_EXTRACT, StepState::Running);
    this->postDetail(L"");
    std::wstring found;
    std::error_code ec;
    auto badArchive = [&](const std::wstring& msg) {
        DeleteFileW(zip.c_str());
        driveFailed = !this->cancel.load();
        status = msg;
        this->postStep(STEP_EXTRACT, StepState::Failed);
        return false;
    };

    if (src == Source::GDriveCert) {
        const std::wstring dirS = Versions::Dir(v.name, Versions::GDRIVE_SIGNED_SUFFIX);
        std::filesystem::remove_all(dirS, ec);
        this->postStatus(L"Достаю пакет и сертификат из архива...");
        if (!Appx::ExtractEntry(zip, L".appx", SignedAppx(v.name), [&](uint64_t d, uint64_t t) {
                this->postDetail(FormatSize(d) + L" из " + FormatSize(t));
                this->postProgress(Scale(STEP_EXTRACT, d, t), Percent(d, t));
            }, &this->cancel, found, status))
            return badArchive(L"архив с Google Диска: " + status);
        if (!Appx::ExtractEntry(zip, L".cer", SignedCer(v.name), nullptr, &this->cancel, found, status))
            return badArchive(L"архив с Google Диска: " + status);
        std::ofstream(Versions::ExtractedMarker(v.name, Versions::GDRIVE_SIGNED_SUFFIX)) << "ok";
    } else {
        const std::wstring dir = Versions::Dir(v.name, Versions::GDRIVE_SUFFIX);
        const std::wstring appx = root + L"\\" + Versions::FolderName(v.name, Versions::GDRIVE_SUFFIX) + L".appx";
        this->postStatus(L"Достаю пакет из архива...");
        if (!Appx::ExtractEntry(zip, L".appx", appx, [&](uint64_t d, uint64_t t) {
                this->postDetail(FormatSize(d) + L" из " + FormatSize(t));
                this->postProgress(Scale(STEP_EXTRACT, d, t * 2), Percent(d, t * 2));
            }, &this->cancel, found, status))
            return badArchive(L"архив с Google Диска: " + status);
        DeleteFileW(zip.c_str());

        this->postStatus(L"Распаковываю в " + dir);
        std::filesystem::remove_all(L"\\\\?\\" + dir, ec);
        bool unpacked = Appx::Extract(appx, dir, [&](uint64_t d, uint64_t t) {
            this->postDetail(FormatSize(d) + L" из " + FormatSize(t));
            this->postProgress(Scale(STEP_EXTRACT, t + d, t * 2), Percent(t + d, t * 2));
        }, &this->cancel, status);
        if (!unpacked) {
            DeleteFileW(appx.c_str());
            status = L"Распаковка не удалась: " + status;
            this->postStep(STEP_EXTRACT, StepState::Failed);
            return false;
        }
        std::ofstream(Versions::ExtractedMarker(v.name, Versions::GDRIVE_SUFFIX)) << "ok";
        if (Globals::DELETE_APPX) DeleteFileW(appx.c_str());
    }
    DeleteFileW(zip.c_str());   // the archive is never kept
    this->postStep(STEP_EXTRACT, StepState::Done);
    this->postProgress(STEP_TO[STEP_EXTRACT], 100);
    return true;
}

void cMain::InstallWorker(VersionInfo v, Source src) {
    auto finish = [this](const std::wstring& msg, bool ok) {
        this->postStatus(msg);
        this->CallAfter([this, ok] {
            this->busy.store(false);
            this->btn_Main->Enable(true);
            this->btn_Version->Enable(true);
            this->tab_Settings->Enable(true);
            this->refreshMainButton();
            if (!ok && !this->playReady) this->btn_Main->SetCaption(L"ПОВТОРИТЬ");
        });
    };
    auto fail = [&](Step step, const std::wstring& msg) {
        this->postStep(step, StepState::Failed);
        finish(msg, false);
    };

    const bool signedBuild = src == Source::GDriveCert;
    const std::wstring suffix = src == Source::GDriveDev ? Versions::GDRIVE_SUFFIX : L"";
    const std::wstring dir = Versions::Dir(v.name, suffix);
    std::wstring status;

    // ---- Steps 1-2: download + unpack (skipped when already done) ----
    bool haveFiles = signedBuild ? SignedReady(v.name) : Versions::IsExtracted(v.name, suffix);
    if (haveFiles) {
        this->postStep(STEP_DOWNLOAD, StepState::Done);
        this->postStep(STEP_EXTRACT, StepState::Done);
        this->postDetail(L"Версия уже распакована");
        this->postProgress(STEP_TO[STEP_EXTRACT], 100);
    } else if (src == Source::Official) {
        if (!this->DownloadAndExtract(v, status)) { finish(status, false); return; }
    } else {
        bool driveFailed = false;
        if (!this->DownloadGDrive(v, src, status, driveFailed)) {
            finish(L"Google Диск: " + status, false);
            if (driveFailed) {
                // Offer the official build instead (answer 4).
                this->CallAfter([this, status] {
                    int r = wxMessageBox(L"Не удалось получить сборку с Google Диска:\n" + status +
                                         L"\n\nСкачать официальную версию " + std::wstring(Globals::DEFAULT_VERSION) +
                                         L" с серверов Microsoft?",
                                         L"Google Диск недоступен", wxYES_NO | wxICON_WARNING, this);
                    if (r == wxYES) this->startInstall(Source::Official);
                });
            }
            return;
        }
    }

    // ---- Step 3: KeyPatcher ----
    this->postStep(STEP_PATCH, StepState::Running);
    this->postDetail(L"");
    std::wstring patchNote;
    if (signedBuild) {
        patchNote = L"Патч Xbox Live пропущен: подписанную сборку нельзя изменять";
        this->postDetail(patchNote);
    } else if (Globals::APPLY_KEYPATCH) {
        this->postStatus(L"Ищу Minecraft.Windows.exe...");
        std::wstring exe = KeyPatch::FindExecutable(dir);
        if (exe.empty()) { fail(STEP_PATCH, L"В папке версии не найден Minecraft.Windows.exe"); return; }
        this->postStatus(L"Патчу Xbox Live: " + exe);
        std::wstring patchStatus;
        KeyPatch::Result r = KeyPatch::Patch(exe, patchStatus);
        if (r == KeyPatch::Result::Error) { fail(STEP_PATCH, L"Патч не удался: " + patchStatus); return; }
        patchNote = patchStatus;
        this->postDetail(patchStatus);
    } else {
        patchNote = L"Патч Xbox Live выключен в настройках";
    }
    this->postStep(STEP_PATCH, StepState::Done);
    this->postProgress(STEP_TO[STEP_PATCH], 100);

    // ---- Step 4: dependencies + register / install ----
    this->postStep(STEP_REGISTER, StepState::Running);
    this->postStatus(L"Проверяю зависимости (VCLibs, Store.Engagement)...");
    const std::wstring depsDir = Versions::Root() + L"\\dependencies";
    auto onStatus = [this](const std::wstring& s) { this->postStatus(s); };
    auto onProgress = [this](unsigned pct) {
        this->postProgress(STEP_FROM[STEP_REGISTER] + (int)pct * (STEP_TO[STEP_REGISTER] - STEP_FROM[STEP_REGISTER]) / 100,
                           (int)pct);
    };

    bool ok;
    if (signedBuild) {
        std::string manifest;
        if (!Appx::ReadEntry(SignedAppx(v.name), "AppxManifest.xml", manifest, status) ||
            !Installer::EnsureDependenciesFor(manifest, depsDir, onStatus, status)) {
            fail(STEP_REGISTER, L"Зависимости: " + status);
            return;
        }
        ok = Installer::InstallSigned(SignedAppx(v.name), SignedCer(v.name), Globals::DATA_DIR + L"\\backups",
                                      onProgress, onStatus, status);
        if (ok && Globals::DELETE_APPX) {
            // Windows keeps its own copy in WindowsApps.
            std::error_code ec;
            std::filesystem::remove_all(Versions::Dir(v.name, Versions::GDRIVE_SIGNED_SUFFIX), ec);
        }
    } else {
        if (!Installer::EnsureDependencies(dir, depsDir, onStatus, status)) {
            fail(STEP_REGISTER, L"Зависимости: " + status);
            return;
        }
        this->postStatus(L"Регистрирую игру в Windows...");
        ok = Installer::Register(dir, Globals::DATA_DIR + L"\\backups", onProgress, onStatus, status);
    }
    if (!ok) { fail(STEP_REGISTER, status); return; }
    this->postStep(STEP_REGISTER, StepState::Done);
    this->postProgress(100, 100);
    this->postDetail(patchNote);

    std::wstring from = src == Source::Official ? L"" : L" (сборка с Google Диска)";
    bool plain = status == L"Игра зарегистрирована" || status == L"Игра установлена";
    finish(L"Готово! Minecraft " + v.name + from + L" установлен.\n" + patchNote +
           (plain ? L"" : L"\n" + status), true);
}

// --------------------------------------------------------------------------- settings
void cMain::fillSettings() {
    this->chk_GDrive->SetValue(Globals::GDRIVE_ENABLED);
    this->rb_GDriveDev->SetValue(Globals::GDRIVE_MODE == Globals::GDriveMode::Dev);
    this->rb_GDriveCert->SetValue(Globals::GDRIVE_MODE == Globals::GDriveMode::Cert);
    this->rb_GDriveDev->Enable(Globals::GDRIVE_ENABLED);
    this->rb_GDriveCert->Enable(Globals::GDRIVE_ENABLED);
    this->chk_KeyPatch->SetValue(Globals::APPLY_KEYPATCH);
    this->chk_DeleteAppx->SetValue(Globals::DELETE_APPX);
    this->txt_VersionsUrl->SetValue(Globals::VERSIONS_URL);
}

void cMain::OnSave(wxCommandEvent&) {
    Globals::GDRIVE_ENABLED = this->chk_GDrive->GetValue();
    Globals::GDRIVE_MODE = this->rb_GDriveCert->GetValue() ? Globals::GDriveMode::Cert : Globals::GDriveMode::Dev;
    Globals::APPLY_KEYPATCH = this->chk_KeyPatch->GetValue();
    Globals::DELETE_APPX = this->chk_DeleteAppx->GetValue();
    wxString url = this->txt_VersionsUrl->GetValue().Strip(wxString::both);
    if (!url.IsEmpty()) Globals::VERSIONS_URL = url.ToStdWstring();

    this->cfg.save();
    this->fillSettings();
    this->selectVersion(this->selected);   // source may have changed
    this->setStatus(L"Настройки сохранены");
    this->ShowPage(false);
}
