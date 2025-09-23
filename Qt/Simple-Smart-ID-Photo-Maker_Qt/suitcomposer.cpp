#include "suitcomposer.h"
#include <QImage>
#include <QFileInfo>
using namespace cv;

/* 생성자: 얼굴 검출용 하르 캐스케이드 로드 */
SuitComposer::SuitComposer(QObject *parent) : QObject{parent}
{
    hasCascade_ =
        faceDet_.load("/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml") ||
        faceDet_.load("/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml") ||
        faceDet_.load("haarcascade_frontalface_default.xml");
    if(!hasCascade_) emit warn("face cascade not found");
}

/* 출력 캔버스 크기와 목 절단선 설정 */
void SuitComposer::setCanvas(int w,int h,int neckY){ W_=w; H_=h; neckY_=neckY; }

/* 수트 PNG 로드(RGBA 보장, 크기 보정) */
bool SuitComposer::loadSuit(const QString& path){
    Mat m = imread(path.toStdString(), IMREAD_UNCHANGED);      // 8UC4 선호
    if(m.empty()){ emit error(QString("suit load fail: %1").arg(path)); return false; }
    if(m.channels()==3){                                       // 알파 없으면 추가(255)
        std::vector<Mat> ch; split(m, ch);
        ch.push_back(Mat(m.size(), CV_8U, Scalar(255)));
        merge(ch, m);
    }
    if(m.cols!=W_ || m.rows!=H_) resize(m, m, Size(W_, H_));   // 캔버스 크기 맞춤
    suitRGBA_ = std::move(m);
    emit info(QString("suit: %1").arg(QFileInfo(path).fileName()));
    return true;
}

/* 가이드 PNG 로드(옵션). 알파 전부 0이면 비활성화 */
bool SuitComposer::loadGuide(const QString& path){
    Mat g = imread(path.toStdString(), IMREAD_UNCHANGED);
    guideOK_ = false;
    if(g.empty()){ emit warn(QString("guide load fail: %1").arg(path)); return false; }
    if(g.channels()==3){                                       // 알파 강제 추가
        std::vector<Mat> ch; split(g, ch);
        ch.push_back(Mat(g.size(), CV_8U, Scalar(255)));
        merge(ch, g);
    }
    if(g.cols!=W_ || g.rows!=H_) resize(g, g, Size(W_, H_));
    std::vector<Mat> ch; split(g, ch);
    if(countNonZero(ch[3])==0){                                // 알파가 전부 0 → 사용 안 함
        emit warn("guide alpha all zero. overlay off"); return false;
    }
    guideRGBA_ = std::move(g);
    guideOK_ = true;
    emit info(QString("guide: %1").arg(QFileInfo(path).fileName()));
    return true;
}

/* 미러/가이드 표시/불투명도 설정 */
void SuitComposer::setMirror(bool on){ mirror_=on; }
void SuitComposer::setGuideVisible(bool on){ showGuide_=on; }
void SuitComposer::setGuideOpacity(double a01){ guideOpacity_=std::clamp(a01,0.0,1.0); }

/* 프리뷰용: 입력 BGR → 미러/리사이즈 → 가이드 오버레이 후 BGR 반환 */
cv::Mat SuitComposer::makePreviewBGR(const cv::Mat& frameBGR) const{
    Mat view;
    if(mirror_) flip(frameBGR, view, 1); else view = frameBGR.clone();
    resize(view, view, Size(W_, H_));
    if(showGuide_ && guideOK_){
        Mat tmp = view.clone();
        overlayRGBA(tmp, guideRGBA_, guideOpacity_);           // BGR 위 RGBA 오버레이
        return tmp;
    }
    return view;
}

/* 가장 큰 얼굴 검출(없으면 빈 Rect) */
cv::Rect SuitComposer::detectLargestFace(const cv::Mat& viewBGR, cv::CascadeClassifier* det) {
    if(!det) return {};
    std::vector<cv::Rect> faces; cv::Mat gray;
    cv::cvtColor(viewBGR, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);
    det->detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(60,60));
    if(faces.empty()) return {};
    int idx=0; for(int i=1;i<(int)faces.size();++i) if(faces[i].area()>faces[idx].area()) idx=i;
    Rect r = faces[idx] & Rect(0,0,viewBGR.cols, viewBGR.rows);
    return r.area()>0 ? r : Rect();
}

/* 합성 파이프라인: GrabCut 알파 → 목 절단 → 경계 정리 → 수트⊕얼굴 RGBA */
cv::Mat SuitComposer::composeRGBA(const cv::Mat& frameBGR){
    CV_Assert(!suitRGBA_.empty());
    Mat view;
    if(mirror_) flip(frameBGR, view, 1); else view = frameBGR.clone();
    resize(view, view, Size(W_, H_));

    // 얼굴 영역 초기값
    Rect face = hasCascade_ ? detectLargestFace(view, &faceDet_) : Rect();
    if(face.area()==0){                                        // 미검출 시 중앙 박스
        int fw = int(W_*0.45), fh=int(H_*0.5);
        face = Rect((W_-fw)/2, (H_-fh)/2, fw, fh);
    }

    // GrabCut 기반 알파 생성
    Mat tri = buildTrimap(view.size(), face, true);
    Mat alpha; makeAlphaByGrabCut(view, tri, alpha, 6);

    // 목선 이하 제거
    if(neckY_>=0 && neckY_<alpha.rows) alpha.rowRange(neckY_, alpha.rows).setTo(0);

    // 경계 정리: 이진화 → CLOSE → ERODE
    threshold(alpha, alpha, 127,255, THRESH_BINARY);
    Mat k3 = getStructuringElement(MORPH_ELLIPSE, Size(3,3));
    morphologyEx(alpha, alpha, MORPH_CLOSE, k3, Point(-1,-1), 1);
    erode(alpha, alpha, k3, Point(-1,-1), 1);

    // 얼굴 RGBA 구성
    std::vector<Mat> bgr; split(view, bgr);
    Mat faceRGBA; merge(std::vector<Mat>{bgr[0], bgr[1], bgr[2], alpha}, faceRGBA);

    // 수트 ⊕ 얼굴 합성(비프리멀티플라이)
    Mat out; alphaOverRGBA(suitRGBA_, faceRGBA, out);
    return out; // 8UC4
}

/* 비프리멀티플라이 RGBA 오버 연산: out = fg ⊕ bg */
void SuitComposer::alphaOverRGBA(const Mat& fgRGBA,const Mat& bgRGBA, Mat& outRGBA){
    CV_Assert(fgRGBA.type()==CV_8UC4 && bgRGBA.type()==CV_8UC4 && fgRGBA.size()==bgRGBA.size());
    std::vector<Mat> fgc,bgc; split(fgRGBA, fgc); split(bgRGBA, bgc);
    Mat af32,ab32; fgc[3].convertTo(af32, CV_32F, 1.0/255.0); bgc[3].convertTo(ab32, CV_32F, 1.0/255.0);
    Mat outA32 = af32 + ab32.mul(1.0f - af32);                 // Aout

    std::vector<Mat> out(4);
    for(int c=0;c<3;c++){
        Mat F,B,Fo; fgc[c].convertTo(F, CV_32F); bgc[c].convertTo(B, CV_32F);
        Fo = F.mul(af32) + B.mul(ab32.mul(1.0f - af32));       // 분자
        Mat denom; max(outA32, 1e-6, denom);                   // 0 나눗셈 방지
        Fo = Fo.mul(1.0f/denom);
        Fo.convertTo(out[c], CV_8U);
    }
    outA32.convertTo(out[3], CV_8U, 255.0);
    merge(out, outRGBA);
}

/* BGR 배경 위 RGBA를 opacity로 오버레이(프리뷰용) */
void SuitComposer::overlayRGBA(Mat& bgr, const Mat& rgba, double opacity){
    CV_Assert(bgr.type()==CV_8UC3 && rgba.type()==CV_8UC4 && bgr.size()==rgba.size());
    opacity = std::clamp(opacity,0.0,1.0);
    std::vector<Mat> ch; split(rgba, ch);
    Mat a32; ch[3].convertTo(a32, CV_32F, 1.0); a32 *= opacity; ch[3] = Mat();
    a32.convertTo(ch[3], CV_8U);
    Mat tmp; merge(ch, tmp);

    // 알파 혼합 O = F*a + B*(1-a)
    std::vector<Mat> t; split(tmp, t);
    Mat a; t[3].convertTo(a, CV_32F, 1.0f/255.0f);
    Mat ia; subtract(1.0, a, ia);
    std::vector<Mat> bb; split(bgr, bb);
    for(int c=0;c<3;c++){
        Mat F,B,O; t[c].convertTo(F, CV_32F); bb[c].convertTo(B, CV_32F);
        O = F.mul(a) + B.mul(ia); O.convertTo(bb[c], CV_8U);
    }
    merge(bb, bgr);
}

/* GrabCut 트라이맵 생성: 얼굴 타원 FGD, 목은 PR_FGD, 외곽은 BGD */
cv::Mat SuitComposer::buildTrimap(cv::Size sz, const cv::Rect& face, bool hasFace){
    Mat m(sz, CV_8U, Scalar(GC_PR_BGD));
    rectangle(m, Rect(0,0,sz.width, sz.height), Scalar(GC_BGD), 2);
    if(hasFace){
        // 얼굴 타원
        RotatedRect rr(Point2f(face.x + face.width*0.5f, face.y + face.height*0.55f),
                       Size2f(face.width*0.9f, face.height*1.1f), 0.f);
        Mat fg(sz, CV_8U, Scalar(0));
        ellipse(fg, rr, Scalar(255), FILLED, LINE_AA);
        m.setTo(GC_FGD, fg==255);
        // 목 사각형(전경 추정)
        int nx = face.x + int(face.width*0.15);
        int nw = int(face.width*0.7);
        int ny = face.y + int(face.height*0.95);
        int nh = int(face.height*0.6);
        Rect neck = (Rect(nx,ny,nw,nh) & Rect(0,0,sz.width, sz.height));
        if(neck.area()>0) rectangle(m, neck, Scalar(GC_PR_FGD), FILLED);
    }
    return m;
}

/* GrabCut 실행 → 전경(확정/추정)을 255로 하는 이진 알파 반환 */
void SuitComposer::makeAlphaByGrabCut(const Mat& bgr, const Mat& trimap, Mat& alphaOut, int iters){
    Mat mask = trimap.clone(), bgModel, fgModel;
    grabCut(bgr, mask, Rect(), bgModel, fgModel, iters, GC_INIT_WITH_MASK);
    alphaOut = Mat(bgr.size(), CV_8U, Scalar(0));
    alphaOut.setTo(255, (mask==GC_FGD) | (mask==GC_PR_FGD));
}

/* Mat → QImage 변환(BGR/RGBA 전용) */
QImage SuitComposer::matBGR2QImage(const Mat& bgr){
    CV_Assert(bgr.type()==CV_8UC3);
    QImage img(bgr.data, bgr.cols, bgr.rows, bgr.step, QImage::Format_BGR888);
    return img.copy();                                          // 소유권 분리
}
QImage SuitComposer::matRGBA2QImage(const Mat& rgba){
    CV_Assert(rgba.type()==CV_8UC4);
    QImage img(rgba.data, rgba.cols, rgba.rows, rgba.step, QImage::Format_RGBA8888);
    return img.copy();
}
