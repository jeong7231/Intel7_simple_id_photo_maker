# Intel7 Simple ID Photo Maker

OpenCV 기반의 스마트 증명사진 제작 도구입니다. 웹캠을 이용하여 실시간으로 증명사진을 촬영하고 편집할 수 있습니다.

## 🚀 기능

### Qt GUI 버전 (Simple-Smart-ID-Photo-Maker_Qt)

최종 결과물 입니다. 이 Qt앱을 컴파일한 후 실행하면 프로그램 결과를 볼 수 있습니다.

- **실시간 웹캠 미리보기**: 30fps 실시간 카메라 피드
- **스마트 촬영**: 자동 얼굴 검출 및 가이드 오버레이
- **이미지 편집**:
  - 흑백 변환
  - 좌우 반전
  - 샤프닝 조절 (0-10 단계)
- **다양한 배경색 지원**: 흰색, 파란색, 빨간색 등
- **수트 합성**: 정장 이미지와 얼굴 자동 합성
- **직관적인 UI**: Qt6 기반의 사용자 친화적 인터페이스

### 콘솔 버전 (webcam_to_suit)

정장 합성과 배경 화면 제거만을 따로 분리한 폴더입니다. Qt 앱에 포함되어 있습니다.

- **웹캠 얼굴 + 수트 합성**: OpenCV GrabCut 알고리즘 사용
- **실시간 미러링**: 미러 모드 토글 가능
- **가이드 오버레이**: 반투명 가이드 이미지 표시
- **자동 얼굴 검출**: Haar Cascade를 이용한 얼굴 인식
- **알파 블렌딩**: RGBA 채널을 이용한 자연스러운 합성

## 📁 프로젝트 구조

```
├── Qt/Simple-Smart-ID-Photo-Maker_Qt/    # Qt GUI 애플리케이션
│   ├── main.cpp                          # 메인 진입점
│   ├── main_app.cpp/h                    # 메인 윈도우 & 카메라 캡처
│   ├── photoeditpage.cpp/h               # 이미지 편집 페이지
│   ├── export_page.cpp/h                 # 내보내기 페이지
│   ├── suitcomposer.cpp/h               # 수트 합성 엔진
│   ├── aspectratiolabel.cpp/h           # 비율 유지 라벨
│   ├── *.ui                             # Qt Designer UI 파일
│   └── Simple-Smart-ID-Photo-Maker_Qt.pro # qmake 프로젝트 파일
├── webcam_to_suit/                       # 콘솔 기반 도구
│   ├── webcam_to_suit.cpp               # 메인 소스 코드
│   ├── Makefile                         # 빌드 설정
│   ├── image/                           # 수트 이미지 폴더
│   └── result/                          # 결과 이미지 저장 폴더
└── README.md                            # 프로젝트 설명서
```

## 🛠️ 시스템 요구사항

### 필수 의존성
- **Qt 6.x**: GUI 프레임워크 (widgets 모듈 포함)
- **OpenCV 4.x**: 컴퓨터 비전 라이브러리
- **C++17**: 표준 라이브러리 지원
- **웹캠**: USB 카메라 또는 내장 카메라
- **pkg-config**: OpenCV 라이브러리 링크용

### 지원 플랫폼
- **Linux**: Ubuntu 18.04+

## 🔧 설치 및 빌드

### Ubuntu/Debian 계열

```bash
# 필수 패키지 설치
sudo apt update
sudo apt install qt6-base-dev qt6-tools-dev libopencv-dev pkg-config build-essential

# 프로젝트 클론
git clone <repository-url>
cd Intel7_simple_id_photo_maker
```

### Qt GUI 버전 빌드

```bash
cd Qt/Simple-Smart-ID-Photo-Maker_Qt/

# qmake로 Makefile 생성
qmake Simple-Smart-ID-Photo-Maker_Qt.pro

# 컴파일
make

# 실행
./Simple-Smart-ID-Photo-Maker_Qt
```

### 콘솔 버전 빌드

```bash
cd webcam_to_suit/

# 컴파일
make

# 실행
make run
# 또는
./webcam_to_suit ./image/man_suit.png
```

## 🎮 사용법

### Qt GUI 버전
1. 애플리케이션 실행
2. 카메라 미리보기에서 얼굴 위치 조정
3. **"사진 촬영"** 버튼 클릭
4. 편집 페이지에서 효과 적용:
   - 흑백 변환 체크박스
   - 좌우 반전 버튼
   - 샤프닝 슬라이더 조절
5. 내보내기 페이지에서 최종 저장

### 콘솔 버전 키보드 조작
- **`c`**: 현재 프레임 캡처 및 합성/저장
- **`g`**: 가이드 표시 토글
- **`m`**: 미러 모드 토글
- **`q` / `ESC`**: 프로그램 종료

## 🖼️ 이미지 처리 기술

### 사용된 OpenCV 알고리즘
1. **GrabCut**: 정확한 전경/배경 분리
2. **Haar Cascade**: 실시간 얼굴 검출
3. **Alpha Blending**: 자연스러운 이미지 합성
4. **Morphological Operations**: 마스크 후처리
5. **Color Space Conversion**: BGR ↔ RGB 변환

### 특별한 처리 기능
- **목선 이하 자동 제거**: Y=290 기준 하단 알파값 0 처리
- **가장자리 정리**: CLOSE → ERODE 연산으로 헤일로 제거
- **비파괴적 편집**: 원본 이미지 보존하며 실시간 미리보기
- **실시간 성능 최적화**: 30fps 미리보기 유지

## 📝 개발 노트

### 아키텍처 설계
- **MVC 패턴**: UI와 비즈니스 로직 분리
- **Signal-Slot**: Qt의 이벤트 기반 통신
- **Timer 기반 프레임 업데이트**: 30ms 간격 갱신
- **상태 관리**: 원본/현재 이미지 분리 저장

### 성능 최적화
- **메모리 효율성**: Mat 객체 재사용
- **CPU 사용량**: 알고리즘 반복 횟수 조절 (GrabCut 6회)
- **실시간 처리**: 카메라 해상도 640x480 고정

# 참여 인원

- 김진형
- 박진수
- 정태윤
