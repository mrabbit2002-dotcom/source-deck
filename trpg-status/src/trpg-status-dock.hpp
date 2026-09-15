#pragma once
#include <QWidget>
#include <QVector>
#include <QString>

class QVBoxLayout;
class QLabel;

class TRPGStatusDock : public QWidget {
  Q_OBJECT
public:
  explicit TRPGStatusDock(QWidget *parent = nullptr);

private:
  struct Character { QString name; int hp = 30; int maxHp = 30; };
  QVector<Character> chars;
  QVBoxLayout *listLayout = nullptr;
  QLabel *statusLabel = nullptr;
  QString htmlPath;

  void addCharacter();
  void removeCharacter(int index);
  void changeHp(int index, int delta);
  void setHp(int index);
  void rebuild();
  void save();
  void load();
  void updateOverlay();
  QString configPath() const;
  QString makeHtml() const;
};
