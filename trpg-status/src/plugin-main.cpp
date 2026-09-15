#include <obs-module.h>
#include <obs-frontend-api.h>
#include "trpg-status-dock.hpp"
OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void){ return "TRPG health status dock and browser HUD for OBS Studio"; }
static TRPGStatusDock *dock=nullptr;
static constexpr const char *DockId="trpg-status-dock";
bool obs_module_load(void){ dock=new TRPGStatusDock(); if(!obs_frontend_add_dock_by_id(DockId,"TRPG Status",dock)){delete dock;dock=nullptr;return false;} blog(LOG_INFO,"[TRPG Status] loaded"); return true; }
void obs_module_unload(void){ if(dock){obs_frontend_remove_dock(DockId);dock=nullptr;} }
