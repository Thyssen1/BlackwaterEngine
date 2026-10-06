#include "Blackwater/Input.h"

#include <algorithm>

namespace
{
    // One wheel "notch" on a standard mouse. Matches Win32's WHEEL_DELTA;
    // restated here so this file needs no Windows header.
    constexpr float kWheelUnitsPerNotch = 120.0f;
}

namespace bw
{
    // ----------------------------------------------------------------------
    // Queries
    // ----------------------------------------------------------------------

    bool Input::IsKeyDown(uint8_t virtualKey) const noexcept
    {
        return m_keysDown[virtualKey];
    }

    bool Input::WasKeyPressed(uint8_t virtualKey) const noexcept
    {
        return m_keysPressed[virtualKey];
    }

    bool Input::IsButtonDown(MouseButton button) const noexcept
    {
        return m_buttonsDown[Index(button)];
    }

    bool Input::WasButtonPressed(MouseButton button) const noexcept
    {
        return m_buttonsPressed[Index(button)];
    }

    bool Input::WasButtonReleased(MouseButton button) const noexcept
    {
        return m_buttonsReleased[Index(button)];
    }

    bool Input::AnyButtonDown() const noexcept
    {
        return std::ranges::any_of(m_buttonsDown, [](bool down) { return down; });
    }

    float Input::WheelNotches() const noexcept
    {
        return static_cast<float>(m_wheelRaw) / kWheelUnitsPerNotch;
    }

    // ----------------------------------------------------------------------
    // Feeders
    // ----------------------------------------------------------------------

    void Input::OnKey(uint8_t virtualKey, bool down) noexcept
    {
        // Holding a key makes Windows send WM_KEYDOWN repeatedly (autorepeat).
        // "Pressed" must mean the up -> down transition only, so a repeat --
        // a down while already down -- is ignored for edge purposes.
        if (down && !m_keysDown[virtualKey])
        {
            m_keysPressed[virtualKey] = true;
        }

        m_keysDown[virtualKey] = down;
    }

    void Input::OnMouseButton(MouseButton button, bool down) noexcept
    {
        const size_t i = Index(button);

        // Pressed and released are separate flags, not derived from IsDown.
        // A quick click can go down AND up between two simulation steps; the
        // next update then sees IsDown == false, but must still see the press.
        if (down && !m_buttonsDown[i])
        {
            m_buttonsPressed[i] = true;
        }
        else if (!down && m_buttonsDown[i])
        {
            m_buttonsReleased[i] = true;
        }

        m_buttonsDown[i] = down;
    }

    void Input::OnMouseMove(int32_t x, int32_t y) noexcept
    {
        // The very first move has no previous position to measure from;
        // reporting a delta from (0, 0) would make the camera lurch on startup.
        if (m_hasMousePosition)
        {
            // Accumulate rather than overwrite: several moves can arrive
            // between two updates, and the update wants their sum.
            m_mouseDeltaX += x - m_mouseX;
            m_mouseDeltaY += y - m_mouseY;
        }

        m_mouseX = x;
        m_mouseY = y;
        m_hasMousePosition = true;
    }

    void Input::OnMouseWheel(int32_t rawDelta) noexcept
    {
        m_wheelRaw += rawDelta;
    }

    void Input::OnFocusLost() noexcept
    {
        m_keysDown.fill(false);
        m_buttonsDown.fill(false);

        // Pending edges are dropped too: an action triggered by a press the
        // user made just before switching away would be a surprise.
        m_keysPressed.fill(false);
        m_buttonsPressed.fill(false);
        m_buttonsReleased.fill(false);
    }

    void Input::EndUpdate() noexcept
    {
        m_keysPressed.fill(false);
        m_buttonsPressed.fill(false);
        m_buttonsReleased.fill(false);

        m_mouseDeltaX = 0;
        m_mouseDeltaY = 0;
        m_wheelRaw    = 0;
    }
}
