#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

class D20DiceDock : public QWidget {
  Q_OBJECT
public:
  explicit D20DiceDock(QWidget *parent = nullptr);

private:
  QLabel *resultLabel{};
  QLabel *totalLabel{};
  QComboBox *dieTypeCombo{};
  QPushButton *addDieButton{};
  QPushButton *rollButton{};
  QPushButton *visibilityButton{};
  QVBoxLayout *diceListLayout{};
  QWidget *diceListWidget{};
  QVector<int> diceSides;
  QVector<int> lastResults;
  QVector<bool> diceLocked;
  QString currentHtmlPath;
  qint64 activeRollNonce{};

  void addDie(int sides);
  void removeDie(int index);
  void toggleDieLock(int index);
  void rebuildDiceList();
  void roll();
  void toggleVisibility();
  void updateVisibilityButton();
  void saveDiceConfig();
  void loadDiceConfig();
  void normalizeResultState();
  QString configPath() const;
  bool ensureDiceSource(const QVector<int> &results, const QVector<bool> &rolling, qint64 nonce);
};
