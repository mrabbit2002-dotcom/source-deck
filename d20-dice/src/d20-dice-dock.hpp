#pragma once

#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class D20DiceDock : public QWidget {
  Q_OBJECT
public:
  explicit D20DiceDock(QWidget *parent = nullptr);

private:
  QLabel *resultLabel{};
  QPushButton *rollButton{};
  QPushButton *visibilityButton{};

  void roll();
  void toggleVisibility();
  void updateVisibilityButton();
  bool ensureDiceSource(int result, qint64 nonce);
};
