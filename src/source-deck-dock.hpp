#pragma once

#include <obs.h>
#include <QGridLayout>
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

private:
  struct Entry {
    QPushButton *button{};
    QString name;
  };

  Mode mode;
  QGridLayout *grid{};
  QTimer timer;
  QVector<Entry> entries;
  bool editMode{false};
  QPushButton *editButton{};
  QPushButton *addButton{};

  void clearDeck();
  void buildControls();
  void addDeckButton(const QString &name, int index);
  void chooseAndAdd();
  void moveEntry(int from, int to);
  void saveLayout();
  void loadLayout();
  QString settingsPath() const;
  QStringList availableItems() const;
  void activateEntry(int index);
  void refreshGrid();
};
