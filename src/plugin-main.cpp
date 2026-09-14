#include <obs-module.h>
#include <obs-frontend-api.h>
#include "source-deck-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("source-deck", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
  return "Scene and source control decks for OBS Studio";
}

static SourceDeckDock *sceneDock = nullptr;
static SourceDeckDock *sourceDock = nullptr;

static constexpr const char *SceneDockId = "source-deck-scenes";
static constexpr const char *SourceDockId = "source-deck-sources";

bool obs_module_load(void)
{
  sceneDock = new SourceDeckDock(SourceDeckDock::Mode::Scenes);
  if (!obs_frontend_add_dock_by_id(SceneDockId, "Scene Deck", sceneDock)) {
    delete sceneDock;
    sceneDock = nullptr;
    return false;
  }

  sourceDock = new SourceDeckDock(SourceDeckDock::Mode::Sources);
  if (!obs_frontend_add_dock_by_id(SourceDockId, "Source Deck", sourceDock)) {
    obs_frontend_remove_dock(SceneDockId);
    sceneDock = nullptr;
    delete sourceDock;
    sourceDock = nullptr;
    return false;
  }

  blog(LOG_INFO, "[Source Deck] Scene Deck and Source Deck loaded");
  return true;
}

void obs_module_unload(void)
{
  if (sourceDock) {
    obs_frontend_remove_dock(SourceDockId);
    sourceDock = nullptr;
  }

  if (sceneDock) {
    obs_frontend_remove_dock(SceneDockId);
    sceneDock = nullptr;
  }
}
