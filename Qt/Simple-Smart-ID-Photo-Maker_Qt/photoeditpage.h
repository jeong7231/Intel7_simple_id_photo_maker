#ifndef PHOTOEDITPAGE_H
#define PHOTOEDITPAGE_H

#include <QWidget>
#include <QMouseEvent>
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>

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
    bool isSpotRemovalMode = false;
    cv::Mat spotSmoothImage;

    bool isTeethWhiteningMode = false;

    cv::CascadeClassifier faceCascade;
    cv::CascadeClassifier eyeCascade;
    cv::Ptr<cv::face::Facemark> facemark;

    cv::Mat displayCurrentImage(cv::Mat& image);
    void applyAllEffects();
    void sharpen(cv::Mat& image, int strength);
    void correctEyes(cv::Mat& image, cv::Rect roi, int strength);
    cv::Rect safeRect(int x, int y, int w, int h, int maxW, int maxH);
    void applySmoothSpot(cv::Mat& image, const cv::Point& center, int radius);
    void applyInpaintSpot(cv::Mat& image, const cv::Point& center, int radius);
    void applyTeethWhitening(cv::Mat& image, const cv::Point& center, int radius);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool drawing = false;
    cv::Point lastPoint;

public slots:
    void loadImage(const QString& imagePath);
private slots:
    void on_BW_Button_clicked(bool checked);
    void on_horizontal_flip_button_clicked();
    void on_Sharpen_bar_actionTriggered(int action);
    void on_eye_size_bar_valueChanged(int value);
    void on_spot_remove_pen_toggled(bool checked);
    void on_teeth_whiten_4_button_clicked(bool checked);

};

#endif // PHOTOEDITPAGE_H
