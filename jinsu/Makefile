CXX = g++
CXXFLAGS = -Wall -std=c++11
LDFLAGS = $(shell pkg-config --cflags --libs opencv4)

TARGET = filter_app
SOURCES = filter.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
