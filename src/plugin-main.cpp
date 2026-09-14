#include <obs-module.h>
#include <obs-frontend-api.h>
#include "source-deck-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("source-deck", "en-US")
MODULE_EXPORT const char *obs_module_description(void) { return "Stream Deck-style source visibility dock for OBS Studio"; }

static SourceDeckDock *dock = nullptr;
static constexpr const char *DockId = "source-deck-dock";

bool obs_module_load(void)
{
  dock = new SourceDeckDock();
  if (!obs_frontend_add_dock_by_id(DockId, "Source Deck", dock)) {
    delete dock;
    dock = nullptr;
    return false;
  }
  blog(LOG_INFO, "[Source Deck] loaded");
  return true;
}

void obs_module_unload(void)
{
  if (dock) {
    obs_frontend_remove_dock(DockId);
    dock = nullptr;
  }
}
