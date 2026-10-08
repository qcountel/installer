#pragma once

#include <wx/colour.h>
#include <wx/font.h>
#include <windows.h>
#include <dwmapi.h>

#include "font_monocraft.h"

// Monochrome "Minecraft"-style theme for Minecraft Installer.
namespace Theme {
    // Base surfaces (black / grey / white only).
    inline const wxColour BG            (23, 23, 23);    // window background
    inline const wxColour TITLEBAR      (14, 14, 14);    // title / tab bar
    inline const wxColour CARD          (29, 29, 29);    // settings cards
    inline const wxColour INPUT_BG      (16, 16, 16);    // text inputs

    // Stone button palette (Minecraft widgets.png feel).
    inline const wxColour STONE         (139, 139, 139); // button face
    inline const wxColour STONE_HOVER   (154, 154, 154);
    inline const wxColour STONE_LIGHT   (198, 198, 198); // top/left bevel
    inline const wxColour STONE_DARK    ( 86,  86,  86); // bottom/right bevel
    inline const wxColour STONE_DISABLED( 74,  74,  74);

    inline const wxColour OUTLINE       (0, 0, 0);       // hard pixel outline

    inline const wxColour FG            (236, 236, 236); // primary text
    inline const wxColour FG_DIM        (138, 138, 138); // secondary text
    inline const wxColour SHADOW        (0, 0, 0);       // text drop shadow

    // Accent is intentionally monochrome (white) for selection highlights.
    inline const wxColour ACCENT        (255, 255, 255);

    // Registers the embedded Monocraft font once per process.
    inline void EnsurePixelFont() {
        static bool done = false;
        if (done) return;
        done = true;
        DWORD count = 0;
        AddFontMemResourceEx((void*)MONOCRAFT_TTF, MONOCRAFT_TTF_LEN, nullptr, &count);
    }

    // Pixel font at the requested point size. Monocraft reads best at multiples of ~9.
    inline wxFont Font(int pt, bool /*bold*/ = false) {
        EnsurePixelFont();
        wxFontInfo info(pt);
        info.FaceName(L"Monocraft");
        return wxFont(info);
    }

    // Windows 10/11: dark title bar matching the window background.
    inline void ApplyDarkTitleBar(HWND hwnd) {
        if (!hwnd) return;
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
        COLORREF caption = RGB(TITLEBAR.Red(), TITLEBAR.Green(), TITLEBAR.Blue());
        DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof(caption));
        COLORREF text = RGB(FG.Red(), FG.Green(), FG.Blue());
        DwmSetWindowAttribute(hwnd, 36 /*DWMWA_TEXT_COLOR*/, &text, sizeof(text));
    }
} // namespace Theme
