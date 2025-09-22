# Makefile for webcam_to_suit_auto.cpp
CXX      = g++
CXXSTD   = -std=c++17

# OpenCV (opencv4 우선, 없으면 opencv)
OPENCV_CFLAGS = $(shell pkg-config --cflags opencv4 2>/dev/null || pkg-config --cflags opencv 2>/dev/null)
OPENCV_LIBS   = $(shell pkg-config --libs   opencv4 2>/dev/null || pkg-config --libs   opencv 2>/dev/null)

CXXFLAGS = -O2 -Wall -Wextra $(OPENCV_CFLAGS)
LDFLAGS  = $(OPENCV_LIBS)

BIN = webcam_to_suit
SRC = webcam_to_suit.cpp

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXSTD) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

run: $(BIN)
	OPENCV_VIDEOIO_PRIORITY_GSTREAMER=0 ./$(BIN) ./image/man_suit.png

clean:
	rm -f $(BIN)
