#include "functions.h"
#include "dispatchers.h"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>

SDispatchResult this_moveFocusTo(std::string args)
{
    dispatchers::dispatch_movefocus(args);
    return {};
}


SDispatchResult this_moveActiveTo(std::string args)
{
    dispatchers::dispatch_movewindow(args);
    return {};
}


eFullscreenMode window_fullscreen_state(PHLWINDOW window)
{
    return window->m_fullscreenState.internal;
}

void toggle_window_fullscreen_internal(PHLWINDOW window, eFullscreenMode mode)
{
    if (window_fullscreen_state(window) != eFullscreenMode::FSMODE_NONE) {
        g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE);
    } else {
        g_pCompositor->setWindowFullscreenInternal(window, mode);
    }
}

WORKSPACEID get_workspace_id()
{
    WORKSPACEID workspace_id;
    if (g_pCompositor->m_lastMonitor->activeSpecialWorkspaceID()) {
        workspace_id = g_pCompositor->m_lastMonitor->activeSpecialWorkspaceID();
    } else {
        workspace_id = g_pCompositor->m_lastMonitor->activeWorkspaceID();
    }
    if (workspace_id == WORKSPACE_INVALID)
        return -1;
    if (g_pCompositor->getWorkspaceByID(workspace_id) == nullptr)
        return -1;

    return workspace_id;
}

void update_relative_cursor_coords(PHLWINDOW window)
{
    if (window != nullptr)
        window->m_relativeCursorCoordsOnLastWarp = g_pInputManager->getMouseCoordsInternal() - window->m_position;
}

void force_focus_to_window(PHLWINDOW window)
{
    g_pInputManager->unconstrainMouse();
    g_pCompositor->focusWindow(window);
    window->warpCursor();

    g_pInputManager->m_forcedFocus = window;
    g_pInputManager->simulateMouseMovement();
    g_pInputManager->m_forcedFocus.reset();
}

void switch_to_window(PHLWINDOW from, PHLWINDOW to)
{
    if (to == nullptr)
        return;

    auto fwid = from != nullptr? from->workspaceID() : WORKSPACE_INVALID;
    auto twid = to->workspaceID();
    bool change_workspace = fwid != twid;
    if (from != to) {
        const PHLWORKSPACE workspace = to->m_workspace;
        eFullscreenMode mode = workspace->m_fullscreenMode;
        if (mode != eFullscreenMode::FSMODE_NONE) {
            if (change_workspace) {
                auto fwindow = workspace->getLastFocusedWindow(); 
                toggle_window_fullscreen_internal(fwindow, eFullscreenMode::FSMODE_NONE);
            } else {
                toggle_window_fullscreen_internal(from, eFullscreenMode::FSMODE_NONE);
            }
        }
        if (change_workspace) {
            // This is to override overview trying to stay in an overview workspace
            g_pCompositor->m_lastMonitor = to->m_monitor;
        }
        force_focus_to_window(to);
        if (mode != eFullscreenMode::FSMODE_NONE) {
            toggle_window_fullscreen_internal(to, mode);
        }
    } else {
        // from and to are the same, it can happen when we want to recover
        // focus after changing to another monitor where focus was lost
        // due to a window exiting in the background
        force_focus_to_window(to);
    }
    return;
}

