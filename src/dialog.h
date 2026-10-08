#pragma once

#include <wx/wx.h>

// Message boxes in the installer's style: dark window, Monocraft font, pixel icon, stone buttons.
namespace Dialog {

    enum class Kind { Info, Warning, Error };

    // Shows a message with a single button.
    void Show(wxWindow* parent, const wxString& title, const wxString& message,
              Kind kind = Kind::Info, const wxString& okLabel = L"ОК");

    // Yes/No question. Closing the window or Esc counts as "no".
    bool Ask(wxWindow* parent, const wxString& title, const wxString& message,
             Kind kind = Kind::Warning, const wxString& yesLabel = L"ДА", const wxString& noLabel = L"НЕТ");

} // namespace Dialog
