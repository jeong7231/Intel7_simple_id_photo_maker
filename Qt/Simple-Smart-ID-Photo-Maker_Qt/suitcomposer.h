#ifndef SUITCOMPOSER_H
#define SUITCOMPOSER_H

#include <QObject>
#include <opencv2/opencv.hpp>

class SuitComposer : public QObject
{
    Q_OBJECT
  public:
    explicit SuitComposer(QObject *parent = nullptr);

    // 고정 캔버스 파라미터S
    void setCanvas(int w, int h, int neckY);
    // 리소스 로드
    bool loadSuit(const QString &path);
    bool loadGuide(const QString &path);

    // 상태 제어
    void setMirror(bool on);
    void setGuideVisible(bool on);
    void setGuideOpacity(double a01);
    void setBackgroundColor(const cv::Scalar &color);

    // 프리뷰 생성: 입력 BGR 프레임 -> 미러/리사이즈/가이드 오버레이된 BGR 반환
    cv::Mat makePreviewBGR(const cv::Mat &frameBGR) const;

    // 얼굴 알파 생성 + 수트 합성 RGBA 반환
    cv::Mat composeRGBA(const cv::Mat &frameBGR);

    // 배경색이 적용된 BGR 이미지 반환 (투명 배경 대신)
    cv::Mat composeBGR(const cv::Mat &frameBGR);

    // 유틸: Mat<->QImage 변환
    static QImage matBGR2QImage(const cv::Mat &bgr);
    static QImage matRGBA2QImage(const cv::Mat &rgba);

  signals:
    void info(const QString &s);
    void warn(const QString &s);
    void error(const QString &s);

  private:
    static void alphaOverRGBA(const cv::Mat &fgRGBA, const cv::Mat &bgRGBA, cv::Mat &outRGBA);
    static void overlayRGBA(cv::Mat &bgr, const cv::Mat &rgba, double opacity);
    static cv::Mat buildTrimap(cv::Size sz, const cv::Rect &face, bool hasFace);
    static void makeAlphaByGrabCut(const cv::Mat &bgr, const cv::Mat &trimap, cv::Mat &alphaOut, int iters = 6);
    static cv::Rect detectLargestFace(const cv::Mat &viewBGR, cv::CascadeClassifier *det);

  private:
    int W_ = 300, H_ = 400, neckY_ = 290;
    bool mirror_ = true;
    bool showGuide_ = true;
    double guideOpacity_ = 0.7;

    cv::Mat suitRGBA_;  // 캔버스 크기 보장
    cv::Mat guideRGBA_; // 옵션
    bool guideOK_ = false;

    cv::CascadeClassifier faceDet_;
    bool hasCascade_ = false;
    cv::Scalar backgroundColor_ = cv::Scalar(255, 255, 255); // 기본 흰색 배경
};

#endif // SUITCOMPOSER_H
