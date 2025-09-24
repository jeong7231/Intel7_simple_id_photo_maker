#include "aspectratiolabel.h"

AspectRatioLabel::AspectRatioLabel(QWidget *parent) : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

int AspectRatioLabel::heightForWidth(int width) const
{
    return static_cast<int>(width * ratio);
}

QSize AspectRatioLabel::sizeHint() const
{
    int w = this->width();
    return QSize(w, heightForWidth(w));
}
