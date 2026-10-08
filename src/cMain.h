#pragma once

#include "config.h"
#include "flatButton.h"
#include "versions.h"

#include <array>
#include <atomic>
#include <thread>
#include <vector>
#include <wx/checkbox.h>

class cMain : public wxFrame {
public:
    enum class StepState { Pending, Running, Done, Failed };
    enum Step { STEP_DOWNLOAD = 0, STEP_EXTRACT, STEP_PATCH, STEP_REGISTER, STEP_COUNT };

    // Tab headers
    FlatButton*   tab_Install = nullptr;
    FlatButton*   tab_Settings = nullptr;

    // Pages
    wxPanel*      pageInstall = nullptr;
    wxPanel*      pageSettings = nullptr;

    // Install page
    FlatButton*   btn_Version = nullptr;  // "ВЕРСИЯ: 1.16.100.4" -> popup menu
    FlatButton*   btn_Main = nullptr;     // "СКАЧАТЬ" / "УСТАНОВИТЬ" / "ИГРАТЬ"
    wxStaticText* lbl_Status = nullptr;

    // Settings page
    wxCheckBox*   chk_KeyPatch = nullptr;
    wxCheckBox*   chk_DeleteAppx = nullptr;
    wxTextCtrl*   txt_VersionsUrl = nullptr;
    FlatButton*   btn_OpenFolder = nullptr;
    FlatButton*   btn_Save = nullptr;

    Config cfg;

    std::thread worker;
    std::thread listLoader;
    std::atomic<bool> busy{ false };
    std::atomic<bool> cancel{ false };

    // Version list (UI thread only). versions[0..] newest first; selected defaults to 1.16.100.4.
    std::vector<VersionInfo> versions;
    VersionInfo selected;
    bool playReady = false;   // selected version is the one registered in Windows

    // Install progress shown on the page (UI thread only).
    std::array<StepState, STEP_COUNT> steps{};
    int progress = 0;        // overall 0..100
    int stepProgress = 0;    // current step 0..100
    wxString stepDetail;     // e.g. "120 МБ из 295 МБ · 5.1 МБ/с · осталось ~40 с"

    cMain();
    virtual ~cMain() override;

    // Navigation
    void ShowPage(bool settings);
    void layoutTabs();
    void layoutInstallPage();

    // Versions
    void OnVersionButton(wxCommandEvent& evt);
    void OnVersionPicked(wxCommandEvent& evt);
    void selectVersion(const VersionInfo& v);
    void refreshMainButton();

    // Install
    void OnMainButton(wxCommandEvent& evt);
    void InstallWorker(VersionInfo v);
    bool DownloadAndExtract(const VersionInfo& v, std::wstring& status);
    void postStatus(const std::wstring& msg);
    void postStep(Step step, StepState st);
    void postProgress(int overall, int step);
    void postDetail(const std::wstring& text);
    void setStatus(const wxString& msg);
    void OnInstallPagePaint(wxPaintEvent& evt);
    void OnClose(wxCloseEvent& evt);

    // Settings
    void OnSave(wxCommandEvent& evt);
    void fillSettings();

    wxDECLARE_EVENT_TABLE();
};
