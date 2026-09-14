#pragma once
#include <obs.h>
#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QTimer>
#include <QString>
#include <QVector>

class SourceDeckDock : public QWidget {
  Q_OBJECT
public:
  explicit SourceDeckDock(QWidget *parent = nullptr);
  ~SourceDeckDock() override = default;
  void rebuild();
  void syncStates();

private:
  enum class DeckType { Scene, Source };
  struct Entry {
    QPushButton *button{};
    DeckType type{DeckType::Source};
    QString name;
  };

  QGridLayout *grid{};
  QTimer timer;
  QVector<Entry> entries;
  QString currentSceneName;
  bool editMode{false};
  QPushButton *editButton{};
  QPushButton *addSceneButton{};
  QPushButton *addSourceButton{};

  void clearDeck();
  void buildControls();
  void addDeckButton(DeckType type, const QString &name, int index);
  void chooseAndAdd(DeckType type);
  void moveEntry(int from, int to);
  void saveLayout();
  void loadLayout();
  QString settingsPath() const;
  QStringList availableScenes() const;
  QStringList availableSources() const;
  void activateEntry(int index);
  void refreshGrid();
};
