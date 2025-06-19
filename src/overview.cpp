#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "overview.h"

extern HANDLE PHANDLE;

inline CFunctionHook* g_pVisibleOnMonitorHook = nullptr;
inline CFunctionHook* g_pRenderLayerHook = nullptr;
inline CFunctionHook* g_pLogicalBoxHook = nullptr;
inline CFunctionHook* g_pRenderSoftwareCursorsForHook = nullptr;
inline CFunctionHook* g_pGetMonitorFromVectorHook = nullptr;
inline CFunctionHook* g_pClosestValidHook = nullptr;

Overview *overviews = nullptr;

typedef bool (*origVisibleOnMonitor)(void *thisptr, PHLMONITOR monitor);
typedef void (*origRenderLayer)(void *thisptr, PHLLS pLayer, PHLMONITOR pMonitor, timespec* time, bool popups);
typedef CBox (*origLogicalBox)(CMonitor *thisptr);
typedef void (*origRenderSoftwareCursorsFor)(void *thisptr, PHLMONITOR pMonitor, timespec* now, CRegion& damage, std::optional<Vector2D> overridePos);
typedef Vector2D (*origClosestValid)(void *thisptr, const Vector2D &pos);
typedef PHLMONITOR (*origGetMonitorFromVector)(void *thisptr, const Vector2D& point);

class OverviewPassElement : public IPassElement {
public:
    struct OverviewModifData {
        std::optional<SRenderModifData> renderModif;
    };

    OverviewPassElement(const OverviewModifData &data) : data(data) {}
    virtual ~OverviewPassElement() = default;

    virtual void draw(const CRegion& damage) {
        if (data.renderModif.has_value())
            g_pHyprOpenGL->m_renderData.renderModif = *data.renderModif;
    }
    virtual bool needsLiveBlur() { return false; };
    virtual bool needsPrecomputeBlur() { return false; };
    virtual bool undiscardable() { return true; };

    virtual const char* passName() {
        return "OverviewPassElement";
    }

private:
    OverviewModifData data;
};

// Needed to show windows that are outside of the viewport
static bool hookVisibleOnMonitor(void *thisptr, PHLMONITOR monitor) {
    CWindow *window = static_cast<CWindow *>(thisptr);
    if (overviews->overview_enabled(window->workspaceID())) {
        return true;
    }
    return ((origVisibleOnMonitor)(g_pVisibleOnMonitorHook->m_original))(thisptr, monitor);
}

// Needed to undo the monitor scale to render layers at the original scale
static void hookRenderLayer(void *thisptr, PHLLS layer, PHLMONITOR monitor, timespec* time, bool popups) {
    WORKSPACEID workspace = monitor->activeSpecialWorkspaceID();
    if (!workspace)
        workspace = monitor->activeWorkspaceID();
    if (overviews->overview_enabled(workspace)) {
        return;
    }
    ((origRenderLayer)(g_pRenderLayerHook->m_original))(thisptr, layer, monitor, time, popups);
}

// Needed to scale the range of the cursor in overview mode to cover the whole area.
static CBox hookLogicalBox(CMonitor *thisptr) {
    WORKSPACEID workspace = thisptr->activeSpecialWorkspaceID();
    if (!workspace)
        workspace = thisptr->activeWorkspaceID();
    return {thisptr->m_position, thisptr->m_size / overviews->get_scale(workspace)};
}

// Needed to render the software cursor only on the correct monitors.
// TODO: what about hardware cursor?
static void hookRenderSoftwareCursorsFor(void *thisptr, PHLMONITOR monitor, timespec* now, CRegion& damage, std::optional<Vector2D> overridePos) {
    // Should render the cursor for all the extent of the workspace, and only on
    // overview workspaces when there is one active, and it is in the current monitor.
    PHLMONITOR last = g_pCompositor->m_lastMonitor.lock();

    if (monitor == last) {
        // Render cursor
        WORKSPACEID workspace = monitor->activeSpecialWorkspaceID();
        if (!workspace)
            workspace = monitor->activeWorkspaceID();
        Vector2D monitor_size = monitor->m_size;
        float scale = monitor->m_scale;
        monitor->m_size = monitor->m_size / scale;
        g_pHyprRenderer->damageMonitor(monitor);
        ((origRenderSoftwareCursorsFor)(g_pRenderSoftwareCursorsForHook->m_original))(thisptr, monitor, now, damage, overridePos);
        monitor->m_size = monitor_size;
    }
}

// Needed to fake an overview monitor's desktop contains all its windows
// instead of some of them being in the other monitor.
static Vector2D hookClosestValid(void *thisptr, const Vector2D& pos) {
    PHLMONITOR last = g_pCompositor->m_lastMonitor.lock();
    WORKSPACEID workspace = last->activeSpecialWorkspaceID();
    if (!workspace)
        workspace = last->activeWorkspaceID();
    bool overview_enabled = overviews->overview_enabled(workspace);
    if (overview_enabled) {
        CBox bounds = last->logicalBox();
        Vector2D ret = pos;
        if (ret.x < bounds.x) ret.x = bounds.x;
        if (ret.x > bounds.x + bounds.w) ret.x = bounds.x + bounds.w;
        if (ret.y < bounds.y) ret.y = bounds.y;
        if (ret.y > bounds.y + bounds.h) ret.y = bounds.y + bounds.h;
        return ret;
    }
    return ((origClosestValid)(g_pClosestValidHook->m_original))(thisptr, pos);
}

// Needed to select the correct monitor for a cursor when two can contain it.
static PHLMONITOR hookGetMonitorFromVector(void *thisptr, const Vector2D& point) {
    CCompositor *compositor = static_cast<CCompositor *>(thisptr);
    // First, see if the current monitor contains the point
    return compositor->m_lastMonitor.lock();
}


Overview::Overview() : initialized(false)
{
    // Hook bool CWindow::visibleOnMonitor(PHLMONITOR pMonitor)
    auto FNS1 = HyprlandAPI::findFunctionsByName(PHANDLE, "visibleOnMonitor");
    if (!FNS1.empty()) {
        g_pVisibleOnMonitorHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS1[0].address, (bool *)hookVisibleOnMonitor);
        if (g_pVisibleOnMonitorHook == nullptr) {
            return;
        }
    } else {
        return;
    }

    auto FNS3 = HyprlandAPI::findFunctionsByName(PHANDLE, "renderLayer");
    if (!FNS3.empty()) {
        g_pRenderLayerHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS3[0].address, (bool *)hookRenderLayer);
        if (g_pRenderLayerHook == nullptr) {
            return;
        }
    } else {
        return;
    }

    auto FNS5 = HyprlandAPI::findFunctionsByName(PHANDLE, "logicalBox");
    if (!FNS5.empty()) {
        g_pLogicalBoxHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS5[0].address, (bool *)hookLogicalBox);
        if (g_pLogicalBoxHook == nullptr) {
            return;
        }
    } else {
        return;
    }

    auto FNS6 = HyprlandAPI::findFunctionsByName(PHANDLE, "renderSoftwareCursorsFor");
    if (!FNS5.empty()) {
        g_pRenderSoftwareCursorsForHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS6[0].address, (bool *)hookRenderSoftwareCursorsFor);
        if (g_pRenderSoftwareCursorsForHook == nullptr) {
            return;
        }
    } else {
        return;
    }

    auto FNS7 =
        HyprlandAPI::findFunctionsByName(PHANDLE, "getMonitorFromVector");
    if (!FNS5.empty()) {
        g_pGetMonitorFromVectorHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS7[0].address, (bool *)hookGetMonitorFromVector);
        if (g_pGetMonitorFromVectorHook == nullptr) {
            return;
        }
    } else {
        return;
    }

    auto FNS8 = HyprlandAPI::findFunctionsByName(PHANDLE, "closestValid");
    if (!FNS5.empty()) {
        g_pClosestValidHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS8[0].address, (bool *)hookClosestValid);
        if (g_pClosestValidHook == nullptr) {
            return;
        }
    } else {
        return;
    }

    initialized = true;
}

Overview::~Overview()
{
    if (overview_enabled()) {
        disable_hooks();
    }

    if (g_pClosestValidHook != nullptr) {
        /* bool success = */HyprlandAPI::removeFunctionHook(PHANDLE, g_pClosestValidHook);
        g_pClosestValidHook = nullptr;
    }

    if (g_pGetMonitorFromVectorHook != nullptr) {
        /* bool success = */HyprlandAPI::removeFunctionHook(PHANDLE, g_pGetMonitorFromVectorHook);
        g_pGetMonitorFromVectorHook = nullptr;
    }

    if (g_pRenderSoftwareCursorsForHook != nullptr) {
        /* bool success = */HyprlandAPI::removeFunctionHook(PHANDLE, g_pRenderSoftwareCursorsForHook);
        g_pRenderSoftwareCursorsForHook = nullptr;
    }

    if (g_pLogicalBoxHook != nullptr) {
        /* bool success = */HyprlandAPI::removeFunctionHook(PHANDLE, g_pLogicalBoxHook);
        g_pLogicalBoxHook = nullptr;
    }

    if (g_pRenderLayerHook != nullptr) {
        /* bool success = */HyprlandAPI::removeFunctionHook(PHANDLE, g_pRenderLayerHook);
        g_pRenderLayerHook = nullptr;
    }

    if (g_pVisibleOnMonitorHook != nullptr) {
        /* bool success = */HyprlandAPI::removeFunctionHook(PHANDLE, g_pVisibleOnMonitorHook);
        g_pVisibleOnMonitorHook = nullptr;
    }

    initialized = false;
}

bool Overview::enable(WORKSPACEID workspace)
{
    if (!initialized)
        return false;
    if (!overview_enabled()) {
        if (!enable_hooks())
            return false;
    }
    workspaces[workspace].overview = true;
    return true;
}

void Overview::disable(WORKSPACEID workspace)
{
    if (!initialized)
        return;
    workspaces[workspace].overview = false;
    workspaces[workspace].scale = 1.0f;
    if (!overview_enabled()) {
        disable_hooks();
    }
}

bool Overview::overview_enabled(WORKSPACEID workspace) const
{
    if (!initialized)
        return false;
    auto enabled = workspaces.find(workspace);
    if (enabled != workspaces.end()) {
        return enabled->second.overview;
    }
    return false;
}

void Overview::set_scale(WORKSPACEID workspace, float scale)
{
    auto enabled = workspaces.find(workspace);
    if (enabled != workspaces.end()) {
        enabled->second.scale = scale;
    }
}

float Overview::get_scale(WORKSPACEID workspace) const
{
    auto enabled = workspaces.find(workspace);
    if (enabled != workspaces.end()) {
        return enabled->second.scale;
    }
    return 1.0f;
}

bool Overview::overview_enabled() const
{
    for (auto workspace : workspaces) {
        if (workspace.second.overview)
            return true;
    }
    return false;
}

bool Overview::enable_hooks()
{
    if (initialized &&
        g_pVisibleOnMonitorHook != nullptr && g_pVisibleOnMonitorHook->hook() &&
        g_pRenderLayerHook != nullptr && g_pRenderLayerHook->hook() &&
        g_pLogicalBoxHook != nullptr && g_pLogicalBoxHook->hook() &&
        g_pRenderSoftwareCursorsForHook != nullptr && g_pRenderSoftwareCursorsForHook->hook() &&
        g_pClosestValidHook != nullptr && g_pClosestValidHook->hook() &&
        g_pGetMonitorFromVectorHook != nullptr && g_pGetMonitorFromVectorHook->hook()) {
        return true;
    }
    return false;
}

void Overview::disable_hooks()
{
    if (!initialized)
        return;

    if (g_pVisibleOnMonitorHook != nullptr) g_pVisibleOnMonitorHook->unhook();
    if (g_pRenderLayerHook != nullptr) g_pRenderLayerHook->unhook();
    if (g_pLogicalBoxHook != nullptr) g_pLogicalBoxHook->unhook();
    if (g_pRenderSoftwareCursorsForHook != nullptr) g_pRenderSoftwareCursorsForHook->unhook();
    if (g_pClosestValidHook != nullptr) g_pClosestValidHook->unhook();
    if (g_pGetMonitorFromVectorHook != nullptr) g_pGetMonitorFromVectorHook->unhook();
}

