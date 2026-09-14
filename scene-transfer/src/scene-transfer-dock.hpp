#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class SceneTransferDock : public QWidget {
  Q_OBJECT
public:
  explicit SceneTransferDock(QWidget *parent = nullptr);

private:
  QComboBox *sceneCombo{};
  QPushButton *refreshButton{};
  QPushButton *exportButton{};
  QPushButton *importButton{};
  QLabel *statusLabel{};

  void refreshScenes();
  void exportScene();
  void importScene();
  void setStatus(const QString &text, bool error = false);
};
