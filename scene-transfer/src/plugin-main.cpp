#include <obs-module.h>
#include <obs-frontend-api.h>
#include "scene-transfer-dock.hpp"

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void)
{
  return "Export and import individual OBS scenes between scene collections";
}

static SceneTransferDock *dock = nullptr;
static constexpr const char *DockId = "scene-transfer-dock";

bool obs_module_load(void)
{
  dock = new SceneTransferDock();
  if (!obs_frontend_add_dock_by_id(DockId, "Scene Transfer", dock)) {
    delete dock;
    dock = nullptr;
    return false;
  }
  blog(LOG_INFO, "[Scene Transfer] loaded");
  return true;
}

void obs_module_unload(void)
{
  if (dock) {
    obs_frontend_remove_dock(DockId);
    dock = nullptr;
  }
}
