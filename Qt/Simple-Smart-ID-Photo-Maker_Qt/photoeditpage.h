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
    cv::Mat getCurrentImage() const;


private:
    Ui::PhotoEditPage *ui;
    main_app* mainApp;
    cv::Mat originalImage;
    cv::Mat currentImage;

    bool isBWMode = false;
    bool isHorizontalFlipped = false;
    int sharpnessStrength = 0;
    int eyeSizeStrength = 0;

    cv::CascadeClassifier faceCascade;
    cv::CascadeClassifier eyeCascade;

    cv::Mat displayCurrentImage(cv::Mat& image);
    void applyAllEffects();
    void sharpen(cv::Mat& image, int strength);
    void correctEyes(cv::Mat& image, cv::Rect roi, int strength);
    cv::Rect safeRect(int x, int y, int w, int h, int maxW, int maxH);

public slots:
    void loadImage(const QString& imagePath);
private slots:
    void on_BW_Button_clicked(bool checked);
    void on_horizontal_flip_button_clicked();
    void on_Sharpen_bar_actionTriggered(int action);
    void on_eye_size_bar_valueChanged(int value);

};

#endif // PHOTOEDITPAGE_H
