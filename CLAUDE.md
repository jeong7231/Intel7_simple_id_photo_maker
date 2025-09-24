# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an OpenCV-based ID photo maker project for Intel 7th generation course, containing two main components:
1. **Qt Application**: A cross-platform GUI application for interactive photo editing
2. **Webcam Suit Composer**: A standalone C++ tool for real-time face+suit composition using computer vision

## Build Commands

### Qt Application (Qt/Simple-Smart-ID-Photo-Maker_Qt/)

This is a Qt6 application using qmake as the build system:

```bash
# Navigate to Qt project directory
cd Qt/Simple-Smart-ID-Photo-Maker_Qt/

# Generate Makefile from .pro file
qmake Simple-Smart-ID-Photo-Maker_Qt.pro

# Build the application
make

# Or build in debug mode
make debug

# Clean build files
make clean
```

The project can also be opened directly in Qt Creator by opening the `.pro` file.

### Webcam Suit Composer (webcam_to_suit/)

This is a standalone C++ application using OpenCV:

```bash
# Navigate to webcam tool directory
cd webcam_to_suit/

# Build using make
make

# Run with default suit image
make run

# Or run manually with custom suit
./webcam_to_suit ./image/custom_suit.png

# Clean build files
make clean
```

## Dependencies

### Qt Application
- **Qt 6.x**: Core GUI framework with widgets module
- **OpenCV 4.x**: Computer vision library for image processing and camera capture
  - Unix: Installed via pkg-config (opencv4)
  - Windows: Manual configuration with includes and libs in C:/opencv/

### Webcam Tool
- **OpenCV 4.x**: Computer vision library with camera capture and image processing
- **GCC with C++17 support**
- **pkg-config** for OpenCV linking

## Architecture Overview

### Qt Application Structure

Multi-page Qt application for interactive ID photo creation:

#### Core Classes

1. **main_app** (`main_app.cpp/h/ui`) - Main window and camera capture
   - Handles webcam initialization and real-time video feed
   - Camera capture functionality using OpenCV VideoCapture
   - Navigation hub for other pages
   - Timer-based frame updates (30ms intervals)

2. **PhotoEditPage** (`photoeditpage.cpp/h/ui`) - Advanced image editing interface
   - Loads captured photos for editing
   - Image processing features:
     - Black & white conversion (`isBWMode`)
     - Horizontal flip (`isHorizontalFlipped`)
     - Sharpness adjustment (`sharpnessStrength` 0-10 range)
     - Teeth whitening functionality
     - Background composition/replacement
   - Applies effects to OpenCV Mat objects
   - Uses `originalImage` and `currentImage` for non-destructive editing

3. **SuitComposer** (`suitcomposer.cpp/h`) - Background composition engine
   - Handles face detection and segmentation
   - GrabCut-based background removal
   - Alpha blending for suit/background composition
   - Background color replacement

4. **export_page** (`export_page.cpp/h/ui`) - Export functionality
   - Handles saving processed images to various formats
   - File dialog integration

#### Application Flow

1. **Camera Capture**: main_app displays live camera feed
2. **Photo Capture**: User takes photo, switches to PhotoEditPage
3. **Image Editing**: Apply effects (BW, flip, sharpening, teeth whitening)
4. **Background Composition**: Replace/modify background using SuitComposer
5. **Export**: Save final image through export_page

#### Key Technical Details

- Uses Qt's signal-slot mechanism for UI interactions
- OpenCV Mat objects for image data manipulation
- Color space conversions (BGR ↔ RGB) for Qt-OpenCV compatibility
- Non-destructive editing with original/current image separation
- Real-time preview updates when applying effects

### Webcam Suit Composer Structure

Standalone real-time face composition tool with advanced computer vision:

#### Key Features

- **Real-time webcam mirroring** with live preview
- **Face detection** using Haar cascades
- **GrabCut-based segmentation** for precise face/neck extraction
- **Alpha compositing** for seamless suit integration
- **Guide overlay system** with adjustable opacity
- **Automatic neck cutoff** at configurable Y coordinate

#### Technical Implementation

- **Alpha blending algorithms**: Non-premultiplied RGBA composition
- **Trimap generation**: Automated foreground/background classification
- **Morphological operations**: Edge cleanup and noise reduction
- **Camera handling**: V4L2 priority with fallback to generic drivers
- **File I/O**: PNG with alpha channel preservation

#### Control Interface

- `c`: Capture current frame and compose with suit
- `g`: Toggle guide overlay visibility
- `m`: Toggle mirror mode
- `q`/`ESC`: Exit application

## File Structure

```
Qt/Simple-Smart-ID-Photo-Maker_Qt/
├── *.cpp, *.h          # Source files
├── *.ui                # Qt Designer UI layouts
├── *.pro               # qmake project file
├── *.ts                # Translation files (Korean)
└── image/              # Asset images

webcam_to_suit/
├── webcam_to_suit.cpp  # Main application source
├── Makefile            # Build configuration
├── image/              # Suit and guide images
└── result/             # Output directory for captures
```

## Internationalization

- Korean translation support via `Simple-Smart-ID-Photo-Maker_Qt_ko_KR.ts`
- Embedded translations in build using Qt's translation system

## Development Notes

- Both applications use OpenCV's BGR color format internally
- Qt-OpenCV integration requires BGR↔RGB conversion for proper display
- Camera initialization prioritizes V4L2 drivers on Linux for better performance
- GrabCut algorithms require careful trimap initialization for optimal results
- Alpha channel handling is critical for proper suit composition