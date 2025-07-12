#ifndef SCROLLER_OVERVIEW_H
#define SCROLLER_OVERVIEW_H

#include <hyprland/src/SharedDefs.hpp>
#include <vector>

class Overview {
public:
    Overview();
    ~Overview();
    bool is_initialized() const { return initialized; }
    bool enable(WORKSPACEID workspace);
    void disable(WORKSPACEID workspace);
    bool overview_enabled(WORKSPACEID workspace) const;
    void set_scale(WORKSPACEID workspace, float scale);

    typedef struct {
	WORKSPACEID workspace;
        bool overview;
        float scale;
        float scale_i; // inverse scale
    } OverviewData;
    OverviewData& data_for(WORKSPACEID workspace);

private:
    bool overview_enabled() const;
    bool enable_hooks();
    void disable_hooks();

    bool initialized;
    std::vector<OverviewData> _workspaceData;
};

#endif // SCROLLER_OVERVIEW_H
