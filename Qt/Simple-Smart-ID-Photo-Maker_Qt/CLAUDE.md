# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

This is a Qt6 application using qmake as the build system. The project uses OpenCV for image processing.

### Building the Application
```bash
# Generate Makefile from .pro file
qmake Simple-Smart-ID-Photo-Maker_Qt.pro

# Build the application
make

# Or build in debug mode
make debug

# Clean build files
make clean
```

### Qt Creator
The project can also be opened directly in Qt Creator by opening the `.pro` file.

## Dependencies

- **Qt 6.x**: Core GUI framework with widgets module
- **OpenCV 4.x**: Computer vision library for image processing and camera capture
  - Unix: Installed via pkg-config (opencv4)
  - Windows: Manual configuration with includes and libs in C:/opencv/

## Architecture Overview

This is a multi-page Qt application for creating ID photos with the following structure:

### Core Classes

1. **main_app** (`main_app.cpp/h/ui`) - Main window and camera capture
   - Handles webcam initialization and real-time video feed
   - Camera capture functionality using OpenCV VideoCapture
   - Navigation hub for other pages
   - Timer-based frame updates (30ms intervals)

2. **PhotoEditPage** (`photoeditpage.cpp/h/ui`) - Image editing interface
   - Loads captured photos for editing
   - Image processing features:
     - Black & white conversion (`isBWMode`)
     - Horizontal flip (`isHorizontalFlipped`)
     - Sharpness adjustment (`sharpnessStrength` 0-10 range)
   - Applies effects to OpenCV Mat objects
   - Uses `originalImage` and `currentImage` for non-destructive editing

3. **export_page** (`export_page.cpp/h/ui`) - Export functionality
   - Handles saving processed images
   - Currently minimal implementation

### Application Flow

1. **Camera Capture**: main_app displays live camera feed
2. **Photo Capture**: User takes photo, switches to PhotoEditPage
3. **Image Editing**: Apply effects (BW, flip, sharpening)
4. **Export**: Navigate to export_page for saving

### Key Technical Details

- Uses Qt's signal-slot mechanism for UI interactions
- OpenCV Mat objects for image data manipulation
- Color space conversions (BGR ↔ RGB) for Qt-OpenCV compatibility
- Non-destructive editing with original/current image separation
- Real-time preview updates when applying effects

### UI Files Structure

All UI layouts are defined in `.ui` files (Qt Designer format):
- `main_app.ui` - Camera interface with capture button
- `photoeditpage.ui` - Editing controls (BW toggle, flip button, sharpness slider)
- `export_page.ui` - Export interface

### Internationalization

- Korean translation support via `Simple-Smart-ID-Photo-Maker_Qt_ko_KR.ts`
- Embedded translations in build