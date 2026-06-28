#pragma once

#include <Windows.h>

#include "AppState.h"

/// @file TrackRenderer.h
/// @brief Paints a piano-roll style visualization of the currently
///        loaded MIDI file onto a child window's device context.

namespace TrackRenderer
{
    /// @brief Paints the piano-roll for every selected track onto
    ///        the given device context. Clears the background first
    ///        and draws one horizontal lane per MIDI note pitch.
    /// @param hdc Destination device context (already BeginPaint'd).
    /// @param clientRect Client area of the piano-roll window.
    /// @param pState Pointer to the shared application state.
    void Paint(HDC hdc, const RECT& clientRect, const AppState* pState);
}
