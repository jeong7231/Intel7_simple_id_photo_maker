#ifndef ASPECTRATIOLABEL_H
#define ASPECTRATIOLABEL_H

#include <QLabel>

class AspectRatioLabel : public QLabel
{
    Q_OBJECT
public:
    explicit AspectRatioLabel(QWidget *parent = nullptr);

    int heightForWidth(int width) const override;
    QSize sizeHint() const override;

private:
    float ratio = 4.0f/3.0f;
};

#endif // ASPECTRATIOLABEL_H
