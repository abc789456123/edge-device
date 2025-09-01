# RTSP Test Client

이 디렉토리는 RTSP 스트림을 테스트하기 위한 클라이언트 애플리케이션입니다.

## 기능

- RTSP 스트림 수신 및 분석
- 실시간 FPS 모니터링
- 프레임 수신 통계
- 연결 상태 디버깅
- 네트워크 문제 진단

## 빌드

```bash
make
```

## 사용법

### 기본 사용 (로컬 스트림 테스트)
```bash
./rtsp_test_client
```

### 원격 스트림 테스트
```bash
./rtsp_test_client rtsp://192.168.1.100:8554/stream
```

### 도움말
```bash
./rtsp_test_client --help
```

## 출력 예시

```
========================================
        RTSP Stream Test Client
========================================
Target URL: rtsp://localhost:8554/stream
========================================
[INFO] Initializing RTSP client for: rtsp://localhost:8554/stream
[INFO] Starting pipeline...
[INFO] Main loop started
[DEBUG] New pad type: application/x-rtp
[INFO] Successfully linked rtspsrc to depayloader
[INFO] Pipeline state changed from READY to PAUSED
[INFO] Pipeline state changed from PAUSED to PLAYING
[STATS] Frames: 30, Elapsed: 1.0s, FPS: 30.0
[STATS] Frames: 60, Elapsed: 2.0s, FPS: 30.0
[SUMMARY] Total frames: 150, Avg FPS: 30.0, Recent FPS: 30.0
```

## 문제 해결

### 연결 실패
- 서버가 실행 중인지 확인
- 방화벽 설정 확인
- 네트워크 연결 확인

### 프레임 수신 안됨
- 스트림 형식 확인 (H.264 지원)
- 네트워크 대역폭 확인
- 인코더 설정 확인

### 낮은 FPS
- 네트워크 지연 확인
- 서버 성능 확인
- 클라이언트 성능 확인
