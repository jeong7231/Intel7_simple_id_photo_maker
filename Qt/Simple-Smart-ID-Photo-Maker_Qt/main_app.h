#ifndef MAIN_APP_H
#define MAIN_APP_H

#include <QWidget>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include "photoeditpage.h"
#include "export_page.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class main_app;
}
QT_END_NAMESPACE

class main_app : public QWidget
{
    Q_OBJECT

public:
    main_app(QWidget *parent = nullptr);
    ~main_app();
    PhotoEditPage *editPage;
    QString currentImagePath;
    export_page *exportPage;

public slots:
    void goToExportPage();

private slots:
    void updateFrame();
    void capturePhoto();

private:
    Ui::main_app *ui;
    cv::VideoCapture camera;
    QTimer *timer;
};
#endif // MAIN_APP_H
