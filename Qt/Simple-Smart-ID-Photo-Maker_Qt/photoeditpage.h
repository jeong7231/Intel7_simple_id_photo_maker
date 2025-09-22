#ifndef PHOTOEDITPAGE_H
#define PHOTOEDITPAGE_H

#include <QWidget>

class main_app;

namespace Ui {
class PhotoEditPage;
}

class PhotoEditPage : public QWidget
{
    Q_OBJECT

public:
    explicit PhotoEditPage(QWidget *parent = nullptr);
    ~PhotoEditPage();
    void setMainApp(main_app* app);

private:
    Ui::PhotoEditPage *ui;
    main_app* mainApp;

public slots:
    void loadImage(const QString& imagePath);
};

#endif // PHOTOEDITPAGE_H
