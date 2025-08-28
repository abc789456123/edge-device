# edge-device

## **`zero_copy_rtsp_demo.cpp`**
libcamera를 통한 zero-copy를 실현하고, 그 프레임 데이터를 GStreamer를 통해 스트리밍하는 코드
- 종속성
    - libcamera
    - Gstreamer

**실제 종속성을 가리키고 있는 패키지명에 관해서는 추가 예정(죄송,,,ㅎㅎ)**

- Config(fixed)
    - resolution : 1080p(1920x1080)
    - fps : 30
    - color : BGR888(stream : srgb)
