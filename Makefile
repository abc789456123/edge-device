# Makefile for Zero-Copy RTSP Streamer

CXX = g++
CXXFLAGS = -std=c++17 -g -O2 -Wall -I/usr/include/libcamera
CXXFLAGS += $(shell pkg-config --cflags gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0)

LDFLAGS = -lcamera -lcamera-base -lpthread
LDFLAGS += $(shell pkg-config --libs gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0)

TARGET = zero_copy_rtsp_streamer
SOURCES = app_main.cpp main.cpp ConfigManager.cpp ZeroCopyCapture.cpp RtspStreamer.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# 의존성 규칙
app_main.o: app_main.cpp main.h
main.o: main.cpp main.h ConfigManager.h ZeroCopyCapture.h RtspStreamer.h
ConfigManager.o: ConfigManager.cpp ConfigManager.h
ZeroCopyCapture.o: ZeroCopyCapture.cpp ZeroCopyCapture.h ConfigManager.h
RtspStreamer.o: RtspStreamer.cpp RtspStreamer.h ConfigManager.h ZeroCopyCapture.h
