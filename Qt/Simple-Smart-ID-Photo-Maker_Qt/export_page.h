#ifndef EXPORT_PAGE_H
#define EXPORT_PAGE_H

#include <QWidget>

namespace Ui {
class export_page;
}

class export_page : public QWidget
{
    Q_OBJECT

public:
    explicit export_page(QWidget *parent = nullptr);
    ~export_page();

private:
    Ui::export_page *ui;
};

#endif // EXPORT_PAGE_H
