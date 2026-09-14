#include <obs-module.h>
#include <obs-frontend-api.h>
#include "d20-dice-dock.hpp"

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void)
{
  return "Animated D20 dice roller dock for OBS Studio";
}

static D20DiceDock *dock = nullptr;
static constexpr const char *DockId = "d20-dice-dock";

bool obs_module_load(void)
{
  dock = new D20DiceDock();
  if (!obs_frontend_add_dock_by_id(DockId, "D20 Dice", dock)) {
    delete dock;
    dock = nullptr;
    return false;
  }
  blog(LOG_INFO, "[D20 Dice] loaded");
  return true;
}

void obs_module_unload(void)
{
  if (dock) {
    obs_frontend_remove_dock(DockId);
    dock = nullptr;
  }
}
