#include "source-deck-dock.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QSignalBlocker>

static QString sceneName()
{
  obs_source_t *scene = obs_frontend_get_current_scene();
  if (!scene) return {};
  QString name = QString::fromUtf8(obs_source_get_name(scene));
  obs_source_release(scene);
  return name;
}

SourceDeckDock::SourceDeckDock(QWidget *parent) : QWidget(parent)
{
  grid = new QGridLayout(this);
  grid->setContentsMargins(8, 8, 8, 8);
  grid->setSpacing(6);
  setLayout(grid);
  rebuild();
  connect(&timer, &QTimer::timeout, this, [this]() {
    const QString now = sceneName();
    if (now != currentSceneName) rebuild(); else syncStates();
  });
  timer.start(250);
}

void SourceDeckDock::clearGrid()
{
  entries.clear();
  while (QLayoutItem *child = grid->takeAt(0)) {
    if (child->widget()) child->widget()->deleteLater();
    delete child;
  }
}

void SourceDeckDock::rebuild()
{
  clearGrid();
  currentSceneName = sceneName();
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  if (!sceneSource) return;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (!scene) { obs_source_release(sceneSource); return; }

  struct Ctx { SourceDeckDock *self; int index; } ctx{this, 0};
  obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *data) {
    auto *ctx = static_cast<Ctx *>(data);
    ctx->self->addSceneItem(item, ctx->index++);
    return true;
  }, &ctx);
  obs_source_release(sceneSource);
}

void SourceDeckDock::addSceneItem(obs_sceneitem_t *item, int index)
{
  obs_source_t *src = obs_sceneitem_get_source(item);
  if (!src) return;
  auto *button = new QPushButton(QString::fromUtf8(obs_source_get_name(src)), this);
  button->setCheckable(true);
  button->setMinimumSize(105, 64);
  button->setChecked(obs_sceneitem_visible(item));
  button->setStyleSheet("QPushButton{font-weight:600;padding:6px;} QPushButton:checked{background:#3a9d5d;color:white;}");
  connect(button, &QPushButton::clicked, this, [item](bool checked) { obs_sceneitem_set_visible(item, checked); });
  constexpr int columns = 3;
  grid->addWidget(button, index / columns, index % columns);
  entries.push_back({button, item});
}

void SourceDeckDock::syncStates()
{
  for (auto &entry : entries) {
    if (!entry.item || !entry.button) continue;
    const bool visible = obs_sceneitem_visible(entry.item);
    if (entry.button->isChecked() != visible) {
      QSignalBlocker blocker(entry.button);
      entry.button->setChecked(visible);
    }
  }
}
