#include <algorithm>
#include <iostream>
#include <opencv2/face.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

using namespace cv;
using namespace cv::face;
using namespace std;

// ================== 전역 변수 ==================
Mat original_image, spot_smoothed_image;
Rect face_roi;
Point last_point;
bool drawing = false;
double display_ratio = 1.0;
Ptr<Facemark> facemark;

// 메인 트랙바 변수
int sharpen_strength = 0;
int bw_on = 0;
int eye_enlargement_strength = 0;
int teeth_whitening_strength = 0;

// 캐스케이드 분류기
CascadeClassifier face_cascade, eye_cascade;

// ================== 함수 선언 ==================
void on_trackbar(int, void *);
void reset_parameters();

// ================== 유틸리티 함수 ==================
bool isValidSize(const Mat &m, int maxSize = 5000)
{
    if (m.empty())
        return false;
    if (m.cols <= 0 || m.rows <= 0)
        return false;
    if (m.cols > maxSize || m.rows > maxSize)
        return false;
    return true;
}

Rect safeRect(int x, int y, int w, int h, int maxW, int maxH)
{
    if (w <= 0 || h <= 0)
        return Rect();
    Rect r(x, y, w, h);
    Rect bounds(0, 0, maxW, maxH);
    r &= bounds;
    if (r.width <= 0 || r.height <= 0)
        return Rect();
    return r;
}

// ================== 핵심 기능 함수 ==================
// 기존의 복잡한 whitenTeeth 함수를 이름 변경 (사용하지 않음)
void _whitenTeethWithFacemarks(cv::Mat &image, Ptr<Facemark> facemark, CascadeClassifier &face_cascade, int strength)
{
    if (strength == 0)
        return; // No whitening if strength is 0

    // 얼굴 검출
    std::vector<Rect> faces;
    face_cascade.detectMultiScale(image, faces, 1.1, 5, 0, Size(80, 80));

    if (faces.empty())
        return;

    for (auto &face : faces)
    {
        // 랜드마크 검출
        std::vector<std::vector<Point2f>> landmarks;
        bool success = facemark->fit(image, std::vector<Rect>{face}, landmarks);
        if (!success || landmarks.empty())
            continue;

        // 입술 영역 (48~67번 포인트)
        std::vector<Point> mouth_points;
        for (int i = 48; i <= 67; i++)
        {
            mouth_points.push_back(landmarks[0][i]);
        }

        // 입술 polygon으로 마스크 만들기
        Mat mouth_mask = Mat::zeros(image.size(), CV_8UC1);
        fillConvexPoly(mouth_mask, mouth_points, Scalar(255));

        // ROI 추출
        Rect mouth_roi = boundingRect(mouth_points);
        Mat mouth_region = image(mouth_roi).clone();
        Mat mask_roi = mouth_mask(mouth_roi);

        // HSV 변환
        Mat hsv;
        cvtColor(mouth_region, hsv, COLOR_BGR2HSV);

        // 치아 후보 (밝고 채도 낮음)
        Mat teeth_mask;
        inRange(hsv, Scalar(0, 0, 120), Scalar(180, 60, 255), teeth_mask);

        // morphology 연산으로 다듬기
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
        morphologyEx(teeth_mask, teeth_mask, MORPH_OPEN, kernel);
        morphologyEx(teeth_mask, teeth_mask, MORPH_CLOSE, kernel);

        // 입술 영역 마스크와 결합
        bitwise_and(teeth_mask, mask_roi, teeth_mask);

        // 마스크 경계 부드럽게
        GaussianBlur(teeth_mask, teeth_mask, Size(15, 15), 5);

        // Lab 색공간 변환 (미백)
        Mat lab;
        cvtColor(mouth_region, lab, COLOR_BGR2Lab);

        std::vector<Mat> lab_planes;
        split(lab, lab_planes);

        // L 채널(밝기) 조절
        // strength를 사용하여 미백 강도 조절 (0-10)
        double whitening_factor = 1.0 + (strength / 10.0) * 0.5;         // 1.0 ~ 1.5 배 밝기 증가
        lab_planes[0].convertTo(lab_planes[0], -1, whitening_factor, 0); // 밝기 채널에 적용
        lab_planes[0].setTo(255, teeth_mask);                            // 치아 부분만 최대 밝기

        merge(lab_planes, lab);
        Mat whitened;
        cvtColor(lab, whitened, COLOR_Lab2BGR);

        // 최종 적용
        whitened.copyTo(image(mouth_roi), teeth_mask);
    }
}

// on_trackbar에서 호출되는 간단한 whitenTeeth 함수
void whitenTeeth(Mat &image, const vector<Point> &mouth_points, int strength)
{
    if (image.empty() || mouth_points.empty() || strength <= 0)
        return;

    // --- 입술 polygon 마스크 생성 ---
    Mat lips_mask = Mat::zeros(image.size(), CV_8UC1);
    fillConvexPoly(lips_mask, mouth_points, Scalar(255));

    // --- ROI 추출 ---
    Rect mouth_roi = boundingRect(mouth_points);
    if (mouth_roi.empty())
        return;
    mouth_roi = mouth_roi & Rect(0, 0, image.cols, image.rows);

    Mat mouth_region = image(mouth_roi).clone();
    Mat lips_roi = lips_mask(mouth_roi);

    // --- HSV 변환 ---
    Mat hsv;
    cvtColor(mouth_region, hsv, COLOR_BGR2HSV);

    // --- 치아 후보: 밝고 채도 낮은 영역 ---
    Mat teeth_mask;
    // Adjusting HSV range for teeth: broader saturation and slightly lower value start
    inRange(hsv, Scalar(0, 0, 100), Scalar(180, 80, 255), teeth_mask);

    // --- 입술 다각형 내부와 교집합 ---
    bitwise_and(teeth_mask, lips_roi, teeth_mask);

    // --- 마스크 정제 ---
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
    morphologyEx(teeth_mask, teeth_mask, MORPH_OPEN, kernel);
    morphologyEx(teeth_mask, teeth_mask, MORPH_CLOSE, kernel);
    GaussianBlur(teeth_mask, teeth_mask, Size(11, 11), 3);

    // --- Lab 색공간으로 변환 ---
    Mat lab;
    cvtColor(mouth_region, lab, COLOR_BGR2Lab);

    float l_strength = strength * 3.0f;
    float b_strength = strength * 2.0f;

    for (int r = 0; r < lab.rows; ++r)
    {
        for (int c = 0; c < lab.cols; ++c)
        {
            if (teeth_mask.at<uchar>(r, c) > 0)
            {
                Vec3b &lab_pixel = lab.at<Vec3b>(r, c);
                lab_pixel[0] = saturate_cast<uchar>(lab_pixel[0] + l_strength); // 밝기 ↑
                lab_pixel[2] = saturate_cast<uchar>(lab_pixel[2] - b_strength); // 노란기 ↓
            }
        }
    }

    // --- 다시 BGR 변환 ---
    Mat whitened;
    cvtColor(lab, whitened, COLOR_Lab2BGR);

    // --- 원본 이미지에 덮어쓰기 ---
    whitened.copyTo(image(mouth_roi), teeth_mask);
}

void sharpen(Mat &image, int strength) { /* 구현 필요 */ }
void correctEyes(Mat &image, Rect roi, int strength) { /* 구현 필요 */ }
void applySmoothSpot(Mat &image, const Point &center, int radius) { /* 구현 필요 */ }

// ================== 콜백 함수 ==================
void on_trackbar(int, void *)
{
    if (spot_smoothed_image.empty() || original_image.empty())
        return;

    Mat current = spot_smoothed_image.clone();

    Rect safe_face = safeRect(face_roi.x, face_roi.y, face_roi.width, face_roi.height, current.cols, current.rows);
    if (!safe_face.empty())
    {
        Mat face_area = current(safe_face);
        if (sharpen_strength > 0)
            sharpen(face_area, sharpen_strength);
    }

    if (teeth_whitening_strength > 0)
    {
        // Convert current image to grayscale for landmark detection
        Mat gray_current;
        cvtColor(current, gray_current, COLOR_BGR2GRAY);

        // Detect landmarks on the face_roi
        std::vector<std::vector<Point2f>> landmarks;
        bool success = facemark->fit(gray_current, std::vector<Rect>{face_roi}, landmarks);

        if (success && !landmarks.empty())
        {
            // Extract mouth points (typically landmarks 48-67 for LBF model)
            std::vector<Point> mouth_points;
            for (int i = 48; i <= 67; i++)
            {
                mouth_points.push_back(landmarks[0][i]);
            }
            whitenTeeth(current, mouth_points, teeth_whitening_strength);
        }
        else
        {
            cerr << "Facial landmark detection failed for teeth whitening.";
        }
    }

    if (eye_enlargement_strength > 0)
    {
        correctEyes(current, face_roi, eye_enlargement_strength);
    }

    if (bw_on > 0)
    {
        cvtColor(current, current, COLOR_BGR2GRAY);
        cvtColor(current, current, COLOR_GRAY2BGR);
    }

    Mat combined;
    try
    {
        hconcat(original_image, current, combined);
    }
    catch (const cv::Exception &e)
    {
        cerr << "[on_trackbar] hconcat failed: " << e.what() << endl;
        return;
    }

    const int MAX_WIDTH = 1600;
    if (combined.cols > MAX_WIDTH)
    {
        display_ratio = (double)MAX_WIDTH / combined.cols;
        resize(combined, combined, Size(), display_ratio, display_ratio, INTER_AREA);
    }
    else
    {
        display_ratio = 1.0;
    }

    imshow("증명사진 보정", combined);
}

void on_mouse(int event, int x, int y, int, void *) { /* 구현 필요 */ }

// ================== 메인 함수 ==================
int main()
{
    original_image = imread("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/jinsu/face.jpeg");
    if (original_image.empty())
    {
        return -1;
    }

    const int MAX_DIMENSION = 1000;
    if (original_image.cols > MAX_DIMENSION || original_image.rows > MAX_DIMENSION)
    {
        double scale = (double)MAX_DIMENSION / max(original_image.cols, original_image.rows);
        resize(original_image, original_image, Size(), scale, scale, INTER_AREA);
    }

    if (!face_cascade.load("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/jinsu/haarcascade_frontalface_default.xml") || !eye_cascade.load("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/jinsu/haarcascade_eye_tree_eyeglasses.xml"))
    {
        return -1;
    }

    facemark = FacemarkLBF::create();
    facemark->loadModel("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/jinsu/lbfmodel.yaml");

    Mat gray;
    cvtColor(original_image, gray, COLOR_BGR2GRAY);
    equalizeHist(gray, gray);
    vector<Rect> faces;
    face_cascade.detectMultiScale(gray, faces, 1.1, 5, 0, Size(80, 80));
    if (!faces.empty())
    {
        face_roi = faces[0];
    }
    else
    {
        face_roi = Rect(0, 0, original_image.cols, original_image.rows);
    }

    spot_smoothed_image = original_image.clone();

    namedWindow("증명사진 보정", WINDOW_AUTOSIZE);
    setMouseCallback("증명사진 보정", on_mouse, NULL);
    createTrackbar("이미지 선명도", "증명사진 보정", &sharpen_strength, 10, on_trackbar);
    createTrackbar("치아 미백", "증명사진 보정", &teeth_whitening_strength, 10, on_trackbar);
    createTrackbar("눈 확대", "증명사진 보정", &eye_enlargement_strength, 10, on_trackbar);
    createTrackbar("흑백", "증명사진 보정", &bw_on, 1, on_trackbar);

    on_trackbar(0, 0);

    while (true)
    {
        int key = waitKey(0);
        if (key == 'q' || key == 27)
            break;
        else if (key == 'r')
        {
            reset_parameters();
            on_trackbar(0, 0);
        }
    }

    destroyAllWindows();
    return 0;
}

void reset_parameters()
{
    spot_smoothed_image = original_image.clone();
    sharpen_strength = 0;
    bw_on = 0;
    eye_enlargement_strength = 0;
    teeth_whitening_strength = 0;

    setTrackbarPos("이미지 선명도", "증명사진 보정", sharpen_strength);
    setTrackbarPos("흑백", "증명사진 보정", bw_on);
    setTrackbarPos("눈 확대", "증명사진 보정", eye_enlargement_strength);
    setTrackbarPos("치아 미백", "증명사진 보정", teeth_whitening_strength);
}
