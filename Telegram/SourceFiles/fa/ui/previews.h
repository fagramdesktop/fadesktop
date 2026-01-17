/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#pragma once

#include "ui/rp_widget.h"

class RoundnessPreview : public Ui::RpWidget
{
public:
  RoundnessPreview(QWidget *parent);

protected:
  void paintEvent(QPaintEvent *e) override;
};

// thx rabbitGram