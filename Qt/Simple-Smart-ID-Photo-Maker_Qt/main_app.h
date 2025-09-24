#ifndef MAIN_APP_H
#define MAIN_APP_H

#include "export_page.h"
#include "photoeditpage.h"
#include "suitcomposer.h"
#include <QResizeEvent>
#include <QTimer>
#include <QWidget>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui
{
class main_app;
}
QT_END_NAMESPACE

class main_app : public QWidget
{
    Q_OBJECT
  public:
    explicit main_app(QWidget *parent = nullptr);
    ~main_app();

    PhotoEditPage *editPage;
    QString currentImagePath;
    export_page *exportPage;

  public slots:
    void goToExportPage();
    void goToExportPageWithImage();

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private slots:
    void updateFrame();  // 프리뷰만 갱신
    void capturePhoto(); // 수트 합성 후 저장
    void on_colorSelect_currentTextChanged(const QString &text);

  private:
    Ui::main_app *ui;
    cv::VideoCapture camera;
    QTimer *timer;

    cv::Mat lastFrameBGR_;              // 최신 원본 프레임
    SuitComposer comp_;                 // 합성 엔진
    cv::Scalar selectedBackgroundColor; // 선택된 배경색 (BGR)
};
#endif // MAIN_APP_H
