#ifndef PHOTOEDITPAGE_H
#define PHOTOEDITPAGE_H

#include <QWidget>
#include <opencv2/opencv.hpp>

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
    cv::Mat originalImage;
    cv::Mat currentImage;

    bool isBWMode = false;
    bool isHorizontalFlipped = false;

    cv::Mat displayCurrentImage(cv::Mat& image);
    void applyAllEffects();

public slots:
    void loadImage(const QString& imagePath);
private slots:
    void on_BW_Button_clicked(bool checked);
    void on_horizontal_flip_button_clicked();
};

#endif // PHOTOEDITPAGE_H
