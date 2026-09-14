#pragma once

#include <obs.h>
#include <QPoint>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

class SourceDeckDock : public QWidget {
  Q_OBJECT

public:
  enum class Mode { Scenes, Sources };

  explicit SourceDeckDock(Mode mode, QWidget *parent = nullptr);
  ~SourceDeckDock() override = default;

  void rebuild();
  void syncStates();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  struct Entry {
    QPushButton *button{};
    QString name;
    QPoint pos{16, 16};
  };

  Mode mode;
  QWidget *canvas{};
  QPushButton *editButton{};
  QPushButton *addButton{};
  QTimer timer;
  QVector<Entry> entries;
  bool editMode{false};
  QString sourceSceneName;

  QPushButton *draggingButton{};
  QPoint dragOffset;

  void buildUi();
  void rebuildButtons();
  void addDeckButton(const QString &name, int index);
  void chooseAndAdd();
  void removeEntry(int index);
  void saveLayout();
  void loadLayout();
  void switchSourceScene(const QString &sceneName);
  QString settingsPath() const;
  QStringList availableItems() const;
  void activateEntry(int index);
  void updateButtonAppearance();
  QPoint clampPosition(const QPoint &pos, const QSize &buttonSize) const;
};
