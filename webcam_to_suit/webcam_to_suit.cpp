/*
 * 웹캠 얼굴 + 수트 합성 도구 (OpenCV)
 *
 * 기능 개요
 * - 웹캠을 미러링하여 미리보기 표시
 * - 가이드 PNG를 반투명 오버레이로 표시/숨김
 * - 캡처 시 GrabCut으로 얼굴/목 영역 알파 추출
 * - 목선 이하 제거 후 가장자리 정리
 * - 수트 PNG(RGBA)와 얼굴 RGBA를 알파 합성하여 미리보기 및 저장
 *
 * 조작키
 *   c : 현재 프레임 캡처 및 합성/저장
 *   g : 가이드 표시 토글
 *   [ / ] : 가이드 불투명도 감소/증가 (코드상 표기는 있으나 실제 키 핸들링은 생략됨)
 *   m : 미러 토글
 *   q 또는 ESC : 종료
 *
 * 입력 인자
 *   argv[1] : 수트 이미지 경로 (기본 ../image/man_suit_bg_remove_3.png)
 *   argv[2] : 가이드 이미지 경로 (기본 ../image/man_suit_bg_remove_3.png)
 *
 * 빌드 예시
 *   g++ -std=c++17 -O2 main.cpp `pkg-config --cflags --libs opencv4` -o webcam_suit
 */

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
using namespace cv;
using namespace std;
namespace fs = std::filesystem;

/* yyyyMMdd_HHmmss 형태의 타임스탬프 문자열 생성 */
static string nowstamp()
{
    time_t t = time(nullptr);
    char b[32];
    strftime(b, sizeof(b), "%Y%m%d_%H%M%S", localtime(&t));
    return b;
}

/*
 * 비프리멀티플라이(Non-premultiplied) RGBA 합성: out = fg ⊕ bg
 * - 입력: 동일 크기의 8UC4(fg, bg)
 * - 출력: outRGBA = 포어그라운드가 백그라운드 위에 얹힌 결과
 * - 수식:
 *     A_out = Af + Ab * (1 - Af)
 *     C_out = (Cf*Af + Cb*Ab*(1 - Af)) / max(A_out, eps)
 *   여기서 A, C는 각각 알파와 RGB 채널, 범위는 [0,1]
 */
static void alphaOverRGBA(const Mat &fgRGBA, const Mat &bgRGBA, Mat &outRGBA)
{
    CV_Assert(fgRGBA.type() == CV_8UC4 && bgRGBA.type() == CV_8UC4 && fgRGBA.size() == bgRGBA.size());

    // 채널 분리: fgc[0..2]=BGR, fgc[3]=A
    vector<Mat> fgc, bgc;
    split(fgRGBA, fgc);
    split(bgRGBA, bgc);

    // 알파를 float [0,1]로 변환
    Mat af32, ab32;
    fgc[3].convertTo(af32, CV_32F, 1.0 / 255.0);
    bgc[3].convertTo(ab32, CV_32F, 1.0 / 255.0);

    // A_out = Af + Ab * (1 - Af)
    Mat outA32 = af32 + ab32.mul(1.0f - af32);

    vector<Mat> out(4);
    for (int c = 0; c < 3; c++)
    {
        // 컬러 채널을 float로
        Mat F, B, Fo;
        fgc[c].convertTo(F, CV_32F);
        bgc[c].convertTo(B, CV_32F);

        // 분자: Cf*Af + Cb*Ab*(1 - Af)
        Fo = F.mul(af32) + B.mul(ab32.mul(1.0f - af32));

        // 0으로 나눔 방지용 분모 클램프
        Mat denom;
        max(outA32, 1e-6, denom);

        // 최종 컬러 = 분자 / 분모
        Fo = Fo.mul(1.0f / denom);

        // 8비트로 환원
        Fo.convertTo(out[c], CV_8U);
    }

    // 최종 알파를 [0,255]로 환산
    outA32.convertTo(out[3], CV_8U, 255.0);

    merge(out, outRGBA);
}

/*
 * BGR 이미지 위에 RGBA를 불투명도(opacity) 조절하여 오버레이
 * - 입력: bgr(8UC3), rgba(8UC4)
 * - opacity는 추가 전역 스케일로 적용되어 최종 알파 = A * opacity
 * - 결과는 bgr에 곱연산 형태로 바로 반영
 */
static void overlayRGBA(Mat &bgr, const Mat &rgba, double opacity)
{
    CV_Assert(bgr.type() == CV_8UC3 && rgba.type() == CV_8UC4 && bgr.size() == rgba.size());

    // 0~1 범위로 제한
    opacity = std::clamp(opacity, 0.0, 1.0);

    // RGBA 분리 후 알파에 opacity 적용
    vector<Mat> ch;
    split(rgba, ch);
    Mat a32;
    ch[3].convertTo(a32, CV_32F, 1.0);
    a32 *= opacity;
    a32.convertTo(ch[3], CV_8U);

    // 수정된 알파로 병합
    Mat tmp;
    merge(ch, tmp);

    // 다시 분리
    vector<Mat> t;
    split(tmp, t);

    // 알파를 [0,1] float
    Mat a;
    t[3].convertTo(a, CV_32F, 1.0f / 255.0f);
    Mat ia;
    subtract(1.0, a, ia); // ia = 1 - a

    // BGR 각각에 오버레이: O = F*a + B*(1-a)
    vector<Mat> bb;
    split(bgr, bb);
    for (int c = 0; c < 3; c++)
    {
        Mat F, B, O;
        t[c].convertTo(F, CV_32F);
        bb[c].convertTo(B, CV_32F);
        O = F.mul(a) + B.mul(ia);
        O.convertTo(bb[c], CV_8U);
    }
    merge(bb, bgr);
}

/*
 * GrabCut용 초기 트라이맵(마스크) 생성
 * - 기본값: 전역을 GC_PR_BGD(배경 추정)로 초기화
 * - 프레임 가장자리를 GC_BGD로 지정해 배경 가중
 * - 얼굴 rect가 있으면 얼굴은 타원으로 GC_FGD(전경 확정)
 * - 목 영역은 GC_PR_FGD(전경 추정)로 확장
 */
static Mat buildTrimap(Size sz, const Rect &face, bool hasFace)
{
    Mat m(sz, CV_8U, Scalar(GC_PR_BGD));                              // 전역: 배경 추정
    rectangle(m, Rect(0, 0, sz.width, sz.height), Scalar(GC_BGD), 2); // 가장자리: 배경 확정

    if (hasFace)
    {
        // 얼굴 중심 기준 타원 전경 확정
        RotatedRect rr(Point2f(face.x + face.width * 0.5f, face.y + face.height * 0.55f), Size2f(face.width * 0.9f, face.height * 1.1f), 0.f);
        Mat fg(sz, CV_8U, Scalar(0));
        ellipse(fg, rr, Scalar(255), FILLED, LINE_AA);
        m.setTo(GC_FGD, fg == 255);

        // 목 영역은 전경 추정으로 보강
        int nx = face.x + int(face.width * 0.15);
        int nw = int(face.width * 0.7);
        int ny = face.y + int(face.height * 0.95);
        int nh = int(face.height * 0.6);
        Rect neck = (Rect(nx, ny, nw, nh) & Rect(0, 0, sz.width, sz.height));
        if (neck.area() > 0)
            rectangle(m, neck, Scalar(GC_PR_FGD), FILLED);
    }
    return m;
}

/*
 * GrabCut으로 알파 마스크 생성
 * - 입력: bgr(8UC3), trimap(GC_* 레이블), 반복 횟수 iters
 * - 출력: alphaOut(8U, 전경=255, 그 외=0)
 * - 후처리(블러 제거)는 외부에서 수행
 */
static void makeAlphaByGrabCut(const Mat &bgr, const Mat &trimap, Mat &alphaOut, int iters = 6)
{
    Mat mask = trimap.clone(), bgModel, fgModel;

    // 마스크 초기화 기반 GrabCut
    grabCut(bgr, mask, Rect(), bgModel, fgModel, iters, GC_INIT_WITH_MASK);

    // 전경 확정+전경 추정을 255로
    alphaOut = Mat(bgr.size(), CV_8U, Scalar(0));
    alphaOut.setTo(255, (mask == GC_FGD) | (mask == GC_PR_FGD));
}

int main(int argc, char **argv)
{
    // 최종 캔버스 크기와 목 절단 기준 Y 좌표
    const int W = 300, H = 400;
    const int neckY = 290; // neckY 이하 알파=0

    fs::create_directories("./result");

    // 수트/가이드 경로 설정
    string suitPath = (argc >= 2) ? argv[1] : "./image/man_suit_bg_remove_3.png";
    string guidePath = (argc >= 3) ? argv[2] : "./image/man_suit_bg_remove_3.png";

    // 수트 PNG 로드 (RGBA 보장)
    Mat suitRGBA = imread(suitPath, IMREAD_UNCHANGED);
    if (suitRGBA.empty())
    {
        fprintf(stderr, "[error] suit load fail: %s\n", suitPath.c_str());
        return 1;
    }
    if (suitRGBA.channels() == 3)
    {
        // 알파가 없으면 255로 추가
        vector<Mat> ch;
        split(suitRGBA, ch);
        ch.push_back(Mat(suitRGBA.size(), CV_8U, Scalar(255)));
        merge(ch, suitRGBA);
    }
    if (suitRGBA.cols != W || suitRGBA.rows != H)
        resize(suitRGBA, suitRGBA, Size(W, H));
    fprintf(stderr, "[info] suit: %s\n", suitPath.c_str());

    // 가이드 PNG 로드 (선택 사항)
    Mat guide = imread(guidePath, IMREAD_UNCHANGED);
    bool guideOK = !guide.empty();
    if (guideOK)
    {
        if (guide.channels() == 3)
        {
            vector<Mat> ch;
            split(guide, ch);
            ch.push_back(Mat(guide.size(), CV_8U, Scalar(255)));
            merge(ch, guide);
        }
        if (guide.cols != W || guide.rows != H)
            resize(guide, guide, Size(W, H));

        // 가이드 알파가 전부 0이면 비활성화
        vector<Mat> gch;
        split(guide, gch);
        if (countNonZero(gch[3]) == 0)
        {
            fprintf(stderr, "[warn] guide alpha all zero. overlay off\n");
            guideOK = false;
        }
        else
            fprintf(stderr, "[info] guide: %s\n", guidePath.c_str());
    }
    else
    {
        fprintf(stderr, "[warn] guide load fail: %s (overlay off)\n", guidePath.c_str());
    }

    bool showGuide = true;     // 가이드 표시 여부
    double guideOpacity = 0.7; // 가이드 불투명도
    bool mirror = true;        // 미러링 여부

    // 카메라 오픈: 우선순위 V4L2(0) -> V4L2(1) -> ANY
    VideoCapture cap(0, CAP_V4L2);
    if (!cap.isOpened())
        cap.open(1, CAP_V4L2);
    if (!cap.isOpened())
        cap.open(0, CAP_ANY);
    if (!cap.isOpened())
    {
        fprintf(stderr, "camera open fail\n");
        return 2;
    }

    // 캡처 설정: MJPG, 640x480
    cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    // 얼굴 검출기 로드(여러 경로 시도)
    CascadeClassifier faceDet;
    bool hasCascade = faceDet.load("/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml") || faceDet.load("/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml") || faceDet.load("haarcascade_frontalface_default.xml");

    Mat frame, view;
    for (;;)
    {
        // 프레임 획득 실패 시 종료
        if (!cap.read(frame))
            break;

        // 미러링 옵션
        if (mirror)
            flip(frame, frame, 1);

        // 표시 크기로 리사이즈
        resize(frame, view, Size(W, H));

        // 미리보기용 복사본
        Mat preview = view.clone();

        // 가이드 오버레이
        if (showGuide && guideOK)
            overlayRGBA(preview, guide, guideOpacity);

        // 도움말 텍스트
        putText(preview, "c=capture  g=guide  [ ] opacity  m=mirror  q=quit", Point(6, H - 10), FONT_HERSHEY_SIMPLEX, 0.45, Scalar(0, 0, 0), 2);

        imshow("preview", preview);

        // 1ms 대기하며 키 입력 확인
        int k = waitKey(1);
        if (k == 'q' || k == 27) // ESC
            break;
        if (k == 'g')
            showGuide = !showGuide;
        if (k == 'm')
            mirror = !mirror;

        // 캡처 처리
        if (k == 'c')
        {
            Rect face;
            bool haveFace = false;

            // 얼굴 검출: 가장 큰 얼굴 선택
            if (hasCascade)
            {
                vector<Rect> faces;
                Mat gray;
                cvtColor(view, gray, COLOR_BGR2GRAY);
                equalizeHist(gray, gray);
                faceDet.detectMultiScale(gray, faces, 1.1, 3, 0, Size(60, 60));
                if (!faces.empty())
                {
                    int idx = 0;
                    for (int i = 1; i < (int)faces.size(); ++i)
                        if (faces[i].area() > faces[idx].area())
                            idx = i;
                    face = faces[idx] & Rect(0, 0, W, H);
                    haveFace = face.area() > 0;
                }
            }

            // 얼굴 미검출 시 중앙 기본 박스 사용
            if (!haveFace)
            {
                int fw = int(W * 0.45), fh = int(H * 0.5);
                face = Rect((W - fw) / 2, (H - fh) / 2, fw, fh);
            }

            // GrabCut 초기 마스크 생성 및 알파 계산
            Mat tri = buildTrimap(view.size(), face, true);
            Mat alpha;
            makeAlphaByGrabCut(view, tri, alpha, 6);

            // 목선 이하 제거
            if (neckY >= 0 && neckY < alpha.rows)
                alpha.rowRange(neckY, alpha.rows).setTo(0);

            // 경계 정리:
            // 1) 이진화로 확정
            // 2) CLOSE(3x3)로 작은 구멍 메움
            // 3) ERODE로 헤일로 줄임(필요시 조절/주석)
            threshold(alpha, alpha, 127, 255, THRESH_BINARY);
            Mat k3 = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
            morphologyEx(alpha, alpha, MORPH_CLOSE, k3, Point(-1, -1), 1);
            erode(alpha, alpha, k3, Point(-1, -1), 1);

            // 얼굴 RGBA 구성
            vector<Mat> bgr;
            split(view, bgr);
            Mat faceRGBA;
            merge(vector<Mat>{bgr[0], bgr[1], bgr[2], alpha}, faceRGBA);

            // 수트 ⊕ 얼굴 합성 (수트가 전경, 얼굴이 배경 역할)
            Mat out;
            alphaOverRGBA(suitRGBA, faceRGBA, out);

            // 미리보기 표시
            Mat showBGR;
            cvtColor(out, showBGR, COLOR_BGRA2BGR);
            imshow("suit preview", showBGR);

            // 파일 저장
            string ts = nowstamp();
            imwrite("./result/suit_" + ts + ".png", out, {IMWRITE_PNG_COMPRESSION, 3});
        }
    }
    return 0;
}
