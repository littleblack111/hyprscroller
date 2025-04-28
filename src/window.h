#ifndef SCROLLER_WINDOW_H
#define SCROLLER_WINDOW_H

#include "common.h"
#include "sizes.h"
#include "decorations.h"

class Window {
public:
    Window(PHLWINDOW window, double maxy, double box_h, StandardSize width);
    ~Window() {
        window->removeWindowDeco(decoration);
    }
    PHLWINDOW get_window() { return window.lock(); }
    double get_geom_h() const { return box_h; }
    void set_geom_h(double geom_h) { box_h = geom_h; }

    void set_geom_x(double x, const Vector2D &gap_x) {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        window->m_position.x = x + topL.x + gap_x.x;
    }
    double get_geom_y(double gap0) const {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        return window->m_position.y - topL.y - gap0;
    }
    void push_fullscreen_geom() {
        push_geom(mem_fs);
    }
    void pop_fullscreen_geom() {
        pop_geom(mem_fs);
    }
    void push_overview_geom() {
        push_geom(mem_ov);
    }
    void pop_overview_geom() {
        pop_geom(mem_ov);
    }
    StandardSize get_height() const { return height; }
    void update_height(StandardSize h, double max);
    void set_height_free() { height = StandardSize::Free; }

    // Called by the parent column on the active window every time it changes width
    // This allows windows to have a independently stored width when they leave
    // the column
    void set_width(StandardSize w) { width = w; }
    StandardSize get_width() const { return width; }
    void set_geom_w(double geomw, const Vector2D &gap_x) {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        geom_w = geomw - topL.x - botR.x - gap_x.x - gap_x.y;
    }
    double get_geom_w(const Vector2D &gap_x) const {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        return geom_w + topL.x + botR.x + gap_x.x + gap_x.y;
    }

    void set_geometry(const Box &box) {
        window->m_position = Vector2D(box.x, box.y);
        window->m_size = Vector2D(box.w, box.h);
        *window->m_realPosition = window->m_position;
        *window->m_realSize = window->m_size;
        window->sendWindowSize();
    }
    bool is_window(PHLWINDOW w) const {
        return window == w;
    }
    
    eFullscreenMode fullscreen_state() const {
        return window->m_fullscreenState.internal;
    }

    void scale(const Vector2D &bmin, const Vector2D &start, double scale, double gap0, double gap1) {
        set_geom_h(get_geom_h() * scale);
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        window->m_position = start + reserved_area.topLeft + (window->m_position - reserved_area.topLeft - bmin) * scale;
        window->m_position.y += gap0;
        window->m_size.x *= scale;
        window->m_size.y = (window->m_size.y + reserved_area.topLeft.y + reserved_area.bottomRight.y + gap0 + gap1) * scale - gap0 - gap1 - reserved_area.topLeft.y - reserved_area.bottomRight.y;
        window->m_size = Vector2D(std::max(window->m_size.x, 1.0), std::max(window->m_size.y, 1.0));
        *window->m_realSize = window->m_size;
        *window->m_realPosition = window->m_position;
        window->sendWindowSize();
    }

    void move_to_bottom(double x, const Box &max, const Vector2D &gap_x, double gap) {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        window->m_position = Vector2D(x + topL.x + gap_x.x, max.y + max.h - get_geom_h() + topL.y + gap);
    }
    void move_to_top(double x, const Box &max, const Vector2D &gap_x, double gap) {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        window->m_position = Vector2D(x + topL.x + gap_x.x, max.y + topL.y + gap);
    }
    void move_to_center(double x, const Box &max, const Vector2D &gap_x, double gap0, double gap1) {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        window->m_position = Vector2D(x + topL.x + gap_x.x, max.y + 0.5 * (max.h - (botR.y - topL.y + gap1 - gap0 + window->m_size.y)));
    }
    void move_to_pos(double x, double y, const Vector2D &gap_x, double gap) {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        window->m_position = Vector2D(x + topL.x + gap_x.x, y + gap + topL.y);
    }

    void scroll(double delta_y) {
        window->m_position.y += delta_y;
        window->m_realPosition->warp(false);
        *window->m_realPosition = window->m_position;
    }

    void update_window(double w, const Vector2D &gap_x, double gap0, double gap1, bool animate) {
        auto reserved = window->getFullWindowReservedArea();
        //win->m_vSize = Vector2D(w - gap_x.x - gap_x.y, wh - gap0 - gap1);
        window->m_size = Vector2D(std::max(w - reserved.topLeft.x - reserved.bottomRight.x - gap_x.x - gap_x.y, 1.0), std::max(get_geom_h() - reserved.topLeft.y - reserved.bottomRight.y - gap0 - gap1, 1.0));
        if (!animate)
            window->m_realPosition->warp(false);
        *window->m_realPosition = window->m_position;
        *window->m_realSize = window->m_size;
        window->sendWindowSize();
    }
    bool can_resize_width(double geomw, double maxw, const Vector2D &gap_x, double gap, double deltax) {
        // First, check if resize is possible or it would leave any window
        // with an invalid size.
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        // Width check
        auto rwidth = geomw + deltax - topL.x - botR.x - gap_x.x - gap_x.y;
        // Now we check for a size smaller than the maximum possible gap, so
        // we never get in trouble when a window gets expelled from a column
        // with gaps_out, gaps_in, to a column with gaps_in on both sides.
        auto mwidth = geomw + deltax - topL.x - botR.x - 2.0 * std::max(std::max(gap_x.x, gap_x.y), gap);
        if (mwidth <= 0.0 || rwidth >= maxw)
            return false;

        return true;
    }
    bool can_resize_height(double maxh, bool active, double gap0, double gap1, double deltay) {
        SBoxExtents reserved_area = window->getFullWindowReservedArea();
        const Vector2D topL = reserved_area.topLeft, botR = reserved_area.bottomRight;
        auto wh = get_geom_h() - gap0 - gap1 - topL.y - botR.y;
        if (active)
            wh += deltay;
        if (wh <= 0.0 || wh + gap0 + gap1 + topL.y + botR.y > maxh)
            return false;
        return true;
    }

    CGradientValueData get_border_color() const;

    void selection_toggle() {
        selected = !selected;
    }

    void selection_set() {
        selected = true;
    }

    void selection_reset() {
        if (selected)
            selection_toggle();
    }

    bool is_selected() const { return selected; }

    void move_to_workspace(PHLWORKSPACE workspace) {
        window->moveToWorkspace(workspace);
        window->m_monitor = workspace->m_monitor;
    }

    void pin(bool pin) {
        if (pin) {
            window->m_tags.applyTag("+scroller:pinned");
        } else {
            window->m_tags.applyTag("-scroller:pinned");
        }
        window->updateDynamicRules();
        g_pCompositor->updateWindowAnimatedDecorationValues(window.lock());
    }

private:
    struct Memory {
        double pos_y;
        double box_h;
        Vector2D position;
        Vector2D size;
    };

    void push_geom(Memory &mem) {
        PHLWINDOW w = window.lock();
        mem.box_h = box_h;
        mem.pos_y = w->m_position.y;
        mem.position = w->m_position;
        mem.size = w->m_size;
    }
    void pop_geom(const Memory &mem) {
        PHLWINDOW w = window.lock();
        box_h = mem.box_h;
        w->m_position.y = mem.pos_y;
        w->m_position = mem.position;
        w->m_size = mem.size;
        *w->m_realPosition = w->m_position;
        *w->m_realSize = w->m_size;
        w->sendWindowSize();
    }

    PHLWINDOWREF window;
    StandardSize height;
    // This keeps track of the window width and recovers it when it is alone
    // in a column. When it is in a column with more windows, the active window
    // has this value synced with the column width. So this value changes when
    // the window is active and resized by its parent column resize.
    StandardSize width;
    // Windows store their `resizeActiveWindow` width, so it can be recovered
    // when their mode changes to StandardSize::FREE. This is necessary because
    // their window->m_vSize changes.
    double geom_w;
    double box_h;
    Memory mem_ov, mem_fs;   // memory to store old height and win y when in overview/fullscreen modes
    bool selected;
    SelectionBorders *decoration;
};

#endif // SCROLLER_WINDOW_H
