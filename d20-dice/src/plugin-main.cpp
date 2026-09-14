#include <obs-module.h>
#include <obs-frontend-api.h>
#include "d20-dice-dock.hpp"

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void)
{
  return "Animated configurable DnD dice roller dock for OBS Studio";
}

static D20DiceDock *dock = nullptr;
static constexpr const char *DockId = "d20-dice-dock";

bool obs_module_load(void)
{
  dock = new D20DiceDock();
  if (!obs_frontend_add_dock_by_id(DockId, "Dice Roller", dock)) {
    delete dock;
    dock = nullptr;
    return false;
  }
  blog(LOG_INFO, "[Dice Roller] loaded");
  return true;
}

void obs_module_unload(void)
{
  if (dock) {
    obs_frontend_remove_dock(DockId);
    dock = nullptr;
  }
}
