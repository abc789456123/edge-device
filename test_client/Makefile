# Makefile for RTSP Test Client

CXX = g++
CXXFLAGS = -std=c++17 -g -O2 -Wall
CXXFLAGS += $(shell pkg-config --cflags gstreamer-1.0)

LDFLAGS = -lpthread
LDFLAGS += $(shell pkg-config --libs gstreamer-1.0)

TARGET = rtsp_test_client
SOURCES = test_client.cpp RtspClient.cpp
OBJECTS = $(SOURCES:.cpp=.o)

SIMPLE_TARGET = simple_test
SIMPLE_SOURCES = simple_test.cpp
SIMPLE_OBJECTS = $(SIMPLE_SOURCES:.cpp=.o)

MANUAL_TARGET = manual_test
MANUAL_SOURCES = manual_test.cpp
MANUAL_OBJECTS = $(MANUAL_SOURCES:.cpp=.o)

.PHONY: all clean simple manual

all: $(TARGET) $(SIMPLE_TARGET) $(MANUAL_TARGET)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(SIMPLE_TARGET): $(SIMPLE_OBJECTS)
	$(CXX) $(SIMPLE_OBJECTS) -o $(SIMPLE_TARGET) $(LDFLAGS)

$(MANUAL_TARGET): $(MANUAL_OBJECTS)
	$(CXX) $(MANUAL_OBJECTS) -o $(MANUAL_TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) $(SIMPLE_OBJECTS) $(SIMPLE_TARGET) $(MANUAL_OBJECTS) $(MANUAL_TARGET)

simple: $(SIMPLE_TARGET)

manual: $(MANUAL_TARGET)

test: $(TARGET)
	./$(TARGET)

test-simple: $(SIMPLE_TARGET)
	./$(SIMPLE_TARGET)

test-manual: $(MANUAL_TARGET)
	./$(MANUAL_TARGET)

test-remote: $(TARGET)
	./$(TARGET) rtsp://192.168.1.100:8554/stream

# 의존성 규칙
test_client.o: test_client.cpp RtspClient.h
RtspClient.o: RtspClient.cpp RtspClient.h
