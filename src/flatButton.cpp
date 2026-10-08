#include "pch.h"
#include "flatButton.h"
#include "theme.h"

FlatButton::FlatButton(wxWindow* parent, wxWindowID id, const wxString& label,
                       const wxPoint& pos, const wxSize& size)
    : wxWindow(parent, id, pos, size, wxBORDER_NONE),
      caption(label),
      colBase(Theme::STONE), colHover(Theme::STONE_HOVER), colText(Theme::FG) {

    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCursor(wxCURSOR_HAND));
    SetFont(Theme::Font(11));

    Bind(wxEVT_PAINT, &FlatButton::OnPaint, this);
    Bind(wxEVT_ENTER_WINDOW, &FlatButton::OnEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &FlatButton::OnLeave, this);
    Bind(wxEVT_LEFT_DOWN, &FlatButton::OnDown, this);
    Bind(wxEVT_LEFT_UP, &FlatButton::OnUp, this);
}

void FlatButton::SetColors(const wxColour& base, const wxColour& hover, const wxColour& text) {
    colBase = base; colHover = hover; colText = text; Refresh();
}

bool FlatButton::Enable(bool enable) {
    bool r = wxWindow::Enable(enable);
    Refresh();
    return r;
}

void FlatButton::OnEnter(wxMouseEvent& e) { hovering = true; Refresh(); e.Skip(); }
void FlatButton::OnLeave(wxMouseEvent& e) { hovering = false; pressed = false; Refresh(); e.Skip(); }
void FlatButton::OnDown(wxMouseEvent& e) { pressed = true; Refresh(); e.Skip(); }

void FlatButton::OnUp(wxMouseEvent& e) {
    bool wasPressed = pressed;
    pressed = false;
    Refresh();
    if (wasPressed && IsEnabled() && GetClientRect().Contains(e.GetPosition())) {
        wxCommandEvent evt(wxEVT_BUTTON, GetId());
        evt.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
    }
    e.Skip();
}

void FlatButton::DrawTextWithShadow(wxDC& dc, const wxString& text, int x, int y,
                                    const wxColour& fg) {
    dc.SetTextForeground(Theme::SHADOW);
    dc.DrawText(text, x + 2, y + 2);
    dc.SetTextForeground(fg);
    dc.DrawText(text, x, y);
}

void FlatButton::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
    dc.Clear();

    wxSize sz = GetClientSize();
    int w = sz.GetWidth(), h = sz.GetHeight();
    dc.SetFont(GetFont());

    // ----- Tab header style -----
    if (style == STYLE_TAB) {
        if (selected) {
            dc.SetBrush(wxBrush(Theme::BG));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(0, 0, w, h);
            // underline
            dc.SetBrush(wxBrush(Theme::STONE_LIGHT));
            dc.DrawRectangle(0, h - 4, w, 4);
        } else if (hovering) {
            dc.SetBrush(wxBrush(Theme::CARD));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(0, 0, w, h);
        }
        wxColour fg = selected ? Theme::FG : Theme::FG_DIM;
        wxCoord tw, th;
        dc.GetTextExtent(caption, &tw, &th);
        DrawTextWithShadow(dc, caption, (w - tw) / 2, (h - th) / 2 - 2, fg);
        return;
    }

    // ----- Stone button style -----
    wxColour face = hovering ? colHover : colBase;
    if (!IsEnabled()) face = Theme::STONE_DISABLED;

    const int b = 4; // bevel thickness in pixels
    bool sunken = pressed && IsEnabled();

    wxColour light = sunken ? Theme::STONE_DARK : Theme::STONE_LIGHT;
    wxColour dark  = sunken ? Theme::STONE_LIGHT : Theme::STONE_DARK;

    // hard black outline
    dc.SetBrush(wxBrush(Theme::OUTLINE));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, w, h);

    // face inside the outline
    dc.SetBrush(wxBrush(face));
    dc.DrawRectangle(3, 3, w - 6, h - 6);

    // top + left light bevel
    dc.SetBrush(wxBrush(light));
    dc.DrawRectangle(3, 3, w - 6, b);          // top
    dc.DrawRectangle(3, 3, b, h - 6);          // left
    // bottom + right dark bevel
    dc.SetBrush(wxBrush(dark));
    dc.DrawRectangle(3, h - 3 - b, w - 6, b);  // bottom
    dc.DrawRectangle(w - 3 - b, 3, b, h - 6);  // right

    if (!caption.empty()) {
        wxColour fg = IsEnabled() ? colText : Theme::FG_DIM;
        wxCoord tw, th;
        dc.GetTextExtent(caption, &tw, &th);
        int ty = (h - th) / 2 - 2 + (sunken ? 1 : 0);
        DrawTextWithShadow(dc, caption, (w - tw) / 2 + (sunken ? 1 : 0), ty, fg);
    }
}