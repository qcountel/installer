#include "pch.h"
#include "dialog.h"
#include "flatButton.h"
#include "theme.h"

#include <utility>
#include <vector>

namespace {

// 12x12 pixel art, scaled 3x. '#' = colour, '.' = transparent.
wxBitmap KindIcon(Dialog::Kind kind) {
    static const char* WARNING[] = {
        ".....##.....",
        "....####....",
        "....#..#....",
        "...##..##...",
        "...##..##...",
        "..###..###..",
        "..###..###..",
        ".##########.",
        ".####..####.",
        "#####..#####",
        "############",
        "............",
    };
    static const char* INFO[] = {
        "..########..",
        ".##########.",
        "#####..#####",
        "#####..#####",
        "############",
        "####...#####",
        "#####..#####",
        "#####..#####",
        "#####..#####",
        "####....####",
        ".##########.",
        "..########..",
    };
    static const char* ERR[] = {
        "..########..",
        ".##########.",
        "##..####..##",
        "###..##..###",
        "####....####",
        "#####..#####",
        "####....####",
        "###..##..###",
        "##..####..##",
        "############",
        ".##########.",
        "..########..",
    };
    const char** art = kind == Dialog::Kind::Warning ? WARNING : kind == Dialog::Kind::Error ? ERR : INFO;
    const wxColour c = kind == Dialog::Kind::Warning ? wxColour(255, 190, 50)
                     : kind == Dialog::Kind::Error   ? wxColour(225, 70, 70)
                                                     : Theme::FG;
    const int n = 12, k = 3;
    wxImage img(n * k, n * k);
    img.InitAlpha();
    for (int y = 0; y < n * k; ++y)
        for (int x = 0; x < n * k; ++x) {
            img.SetRGB(x, y, c.Red(), c.Green(), c.Blue());
            img.SetAlpha(x, y, art[y / k][x / k] == '#' ? 255 : 0);
        }
    return wxBitmap(img);
}

class PixelDialog : public wxDialog {
public:
    PixelDialog(wxWindow* parent, const wxString& title, const wxString& message, Dialog::Kind kind,
                const std::vector<std::pair<int, wxString>>& buttons, int escapeId, int enterId)
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX),
          escape(escapeId), enter(enterId) {
        Theme::EnsurePixelFont();
        this->SetBackgroundColour(Theme::BG);
        Theme::ApplyDarkTitleBar((HWND)this->GetHandle());

        wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* top = new wxBoxSizer(wxHORIZONTAL);
        top->Add(new wxStaticBitmap(this, wxID_ANY, KindIcon(kind)), 0, wxLEFT | wxTOP | wxRIGHT, 20);

        wxBoxSizer* col = new wxBoxSizer(wxVERTICAL);
        wxStaticText* heading = new wxStaticText(this, wxID_ANY, title.Upper());
        heading->SetFont(Theme::Font(10));
        heading->SetForegroundColour(Theme::FG);
        heading->SetBackgroundColour(Theme::BG);
        col->Add(heading, 0, wxBOTTOM, 10);

        wxStaticText* text = new wxStaticText(this, wxID_ANY, message);
        text->SetFont(Theme::Font(8));
        text->SetForegroundColour(Theme::FG_DIM);
        text->SetBackgroundColour(Theme::BG);
        text->Wrap(this->FromDIP(330));
        col->Add(text, 0);
        top->Add(col, 1, wxTOP | wxRIGHT, 20);
        root->Add(top, 1, wxEXPAND);

        wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
        row->AddStretchSpacer(1);
        for (const auto& [id, label] : buttons) {
            FlatButton* b = new FlatButton(this, id, label, wxDefaultPosition, this->FromDIP(wxSize(120, 40)));
            b->SetFont(Theme::Font(10));
            b->Bind(wxEVT_BUTTON, [this, id = id](wxCommandEvent&) { this->EndModal(id); });
            row->Add(b, 0, wxLEFT, 10);
        }
        root->Add(row, 0, wxEXPAND | wxALL, 20);

        this->SetSizerAndFit(root);
        this->SetMinSize(wxSize(this->FromDIP(420), -1));
        this->Fit();
        this->CentreOnParent();

        this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { this->EndModal(this->escape); });
        this->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
            if (e.GetKeyCode() == WXK_ESCAPE) { this->EndModal(this->escape); return; }
            // Enter only confirms plain messages; questions need an explicit click.
            if ((e.GetKeyCode() == WXK_RETURN || e.GetKeyCode() == WXK_NUMPAD_ENTER) && this->enter != wxID_NONE) {
                this->EndModal(this->enter);
                return;
            }
            e.Skip();
        });
        wxBell();
    }

private:
    int escape;
    int enter;
};

} // namespace

namespace Dialog {

void Show(wxWindow* parent, const wxString& title, const wxString& message, Kind kind, const wxString& okLabel) {
    PixelDialog dlg(parent, title, message, kind, { { wxID_OK, okLabel } }, wxID_OK, wxID_OK);
    dlg.ShowModal();
}

bool Ask(wxWindow* parent, const wxString& title, const wxString& message, Kind kind,
         const wxString& yesLabel, const wxString& noLabel) {
    PixelDialog dlg(parent, title, message, kind, { { wxID_YES, yesLabel }, { wxID_NO, noLabel } }, wxID_NO, wxID_NONE);
    return dlg.ShowModal() == wxID_YES;
}

} // namespace Dialog
