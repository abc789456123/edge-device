# Zero-Copy RTSP Streamer (Refactored)

이 프로젝트는 libcamera를 사용하여 카메라에서 프레임을 캡처하고 GStreamer를 통해 RTSP로 스트리밍하는 애플리케이션입니다. 코드가 모듈화되어 유지보수가 쉽도록 리팩토링되었습니다.

## 프로젝트 구조

```
edge-device/
├── config.json              # 설정 파일
├── main.h                   # 메인 애플리케이션 헤더
├── main.cpp                 # 메인 애플리케이션 구현
├── app_main.cpp             # 실행 진입점
├── ConfigManager.h          # 설정 관리자 헤더
├── ConfigManager.cpp        # 설정 관리자 구현
├── ZeroCopyCapture.h        # 카메라 캡처 헤더
├── ZeroCopyCapture.cpp      # 카메라 캡처 구현
├── RtspStreamer.h           # RTSP 스트리머 헤더
├── RtspStreamer.cpp         # RTSP 스트리머 구현
├── Makefile                 # 빌드 설정
└── README_REFACTORED.md     # 이 파일
```

## 모듈 설명

### 1. ConfigManager
- `config.json` 파일에서 설정을 로드
- 비디오 설정 (해상도, FPS, 픽셀 포맷, 버퍼 개수)
- RTSP 설정 (포트, 마운트 포인트, 비트레이트, 인코더, 파이프라인)

### 2. ZeroCopyCapture
- libcamera를 사용한 카메라 프레임 캡처
- DMA 버퍼를 사용한 제로 카피 구현
- 프레임 콜백 메커니즘

### 3. RtspStreamer
- GStreamer를 사용한 RTSP 스트리밍
- 설정 가능한 인코더 및 파이프라인
- 실시간 프레임 전송

### 4. Main Application
- 전체 애플리케이션 관리
- 시그널 처리
- 모듈 간 조정

## 설정 파일 (config.json)

```json
{
    "video": {
        "width": 1920,
        "height": 1080,
        "fps": 30,
        "pixel_format": "BGR888",
        "buffer_count": 8
    },
    "rtsp": {
        "port": 8554,
        "mount_point": "/stream",
        "bitrate": 2000000,
        "encoder": "v4l2h264enc",
        "pipeline": "appsrc name=mysrc ! queue ! v4l2convert ! video/x-raw,format=NV12 ! queue ! v4l2h264enc ! video/x-h264,level=(string)4 ! rtph264pay name=pay0 pt=96"
    }
}
```

## 빌드 방법

### 필요한 의존성
- libcamera
- GStreamer 1.0
- GStreamer RTSP Server
- GStreamer App

### 컴파일
```bash
make
```

또는 직접 컴파일:
```bash
g++ -std=c++17 -g -O2 -Wall -I/usr/include/libcamera \
`pkg-config --cflags gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0` \
-o zero_copy_rtsp_streamer app_main.cpp main.cpp ConfigManager.cpp ZeroCopyCapture.cpp RtspStreamer.cpp \
-lcamera -lcamera-base \
`pkg-config --libs gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0` -lpthread
```

### 정리
```bash
make clean
```

## 실행 방법

```bash
./zero_copy_rtsp_streamer
```

설정 파일이 `config.json`으로 자동 로드됩니다.

## RTSP 스트림 접속

스트림 URL:
```
rtsp://<your-ip-address>:8554/stream
```

VLC 등의 플레이어에서 접속할 수 있습니다.


## 커스터마이징

- `config.json`을 수정하여 해상도, FPS, 인코더 등을 변경 가능
- 새로운 픽셀 포맷 지원을 위해 `ZeroCopyCapture::getPixelFormat()` 수정
- 다른 GStreamer 파이프라인 사용을 위해 설정 파일의 pipeline 항목 수정
