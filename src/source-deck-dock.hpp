#pragma once
#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QTimer>
#include <QString>
#include <vector>

struct obs_sceneitem;
typedef struct obs_sceneitem obs_sceneitem_t;

class SourceDeckDock : public QWidget {
  Q_OBJECT
public:
  explicit SourceDeckDock(QWidget *parent = nullptr);
  ~SourceDeckDock() override = default;
  void rebuild();
  void syncStates();

private:
  struct Entry { QPushButton *button{}; obs_sceneitem_t *item{}; };
  QGridLayout *grid{};
  QTimer timer;
  std::vector<Entry> entries;
  QString currentSceneName;
  void clearGrid();
  void addSceneItem(obs_sceneitem_t *item, int index);
};
