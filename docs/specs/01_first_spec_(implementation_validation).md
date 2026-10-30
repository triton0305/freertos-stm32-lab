# STM32 FreeRTOS Core SW Lab — Codex 구현·검증 명세서

## 0. 프로젝트 목적 및 환경

목적: 차량용 Core SW·Platform SW 직무 대비, STM32F411RE에서 FreeRTOS의 실제 스케줄링과 태스크 간 통신, 인터럽트, 시간 제어를 학습할 수 있는 독립 실습 프로젝트를 완성한다.

현재 작업 디렉터리:

`~/projects/stm32-freertos-lab/`

개발환경:

| 구분 | 환경 |
| --- | --- |
| 개발 PC | Windows + WSL2 Ubuntu |
| WSL 사용자 | kccistc |
| 개발 도구 | Codex, CMake, Ninja, GCC |
| 펌웨어 타깃 | STM32F411RET6, Cortex-M4F |
| 보드 | NUCLEO-F411RE |
| RTOS | FreeRTOS |
| 언어 | C11 |
| ARM Toolchain | arm-none-eabi-gcc |
| Host Toolchain | GCC, MinGW-w64 |
| 디버거 | ST-LINK |
| 통신 | USART2, 115200 8N1 |
| USART2 TX/RX | PA2 / PA3 |
| Windows UART | COM7 |
| LED | LD2, PA5 |
| 버튼 | B1 USER, PC13 |
| CLI TCP | 127.0.0.1:19006 |

현재 Windows PowerShell에서 STMicroelectronics STLink Virtual COM Port가 **COM7**로 인식됨을 확인했다.

단, COM7 인식은 실제 FreeRTOS 펌웨어 동작 검증을 의미하지 않는다.

하드웨어는 NUCLEO-F411RE와 ST-LINK USB만 사용한다. 외부 배선은 사용하지 않는다.

## 1. 가장 먼저 할 일 — 현재 소스 분석

현재 디렉터리에 이미 FreeRTOS Lab 초안 소스가 들어 있다.

먼저 다음 문서를 확인한다.

- `README.md`
- `CODEX_HANDOFF.md`
- `IMPLEMENTATION_REPORT.md`

이후 C 소스, 헤더, CMake, Linker Script, FreeRTOS 설정 및 Host 테스트 코드를 직접 검토한다.

**기존 문서의 구현 완료 및 테스트 PASS 기록을 그대로 신뢰하지 마라.**

소스와 실제 실행 결과를 기준으로 검증 상태를 다시 판정하라.

불완전하거나 잘못된 구현은 수정하고, 누락된 기능은 추가한다.

초안을 무조건 폐기하거나 처음부터 전부 다시 작성할 필요는 없다. 안전하고 재현 가능한 기존 코드는 재사용한다.

## 2. 독립 프로젝트 유지

프로젝트 위치:

`~/projects/stm32-freertos-lab/`

기존 프로젝트:

`~/projects/cpp-equipment-controller/`

두 프로젝트는 완전히 독립적으로 유지한다.

절대 금지:

- 기존 Equipment Controller 소스 수정
- 기존 프로젝트 CMake 변경
- 기존 프로젝트의 HAL/CMSIS 파일에 대한 로컬 경로 의존성
- 기존 프로젝트에서 소스를 복사해 원본을 변경하는 작업
- 새 프로젝트에 불필요한 하드웨어 기능 추가

새 프로젝트는 별도로 Clone하거나 다른 PC로 이동하더라도 독립 빌드할 수 있어야 한다.

FreeRTOS Kernel 및 STM32 HAL/CMSIS 의존성은 공식 배포 소스의 고정 버전 또는 특정 커밋으로 관리한다.

기존 설정이 올바르다면 유지하고, 아니라면 수정한다.

의존성 다운로드 실패와 버전 불일치를 명확하게 처리하라.

## 3. FreeRTOS 펌웨어 요구사항

STM32F411RE에서 다음 기능을 실제 FreeRTOS API로 구현한다.

1. Task 생성 및 Scheduler 시작
2. Priority 기반 Task Scheduling
3. BLOCKED/READY/RUNNING 등 Task 상태 관찰
4. Queue 기반 Task 간 메시지 전달
5. Mutex 기반 공유 상태 보호
6. ISR → Task Notification
7. `vTaskDelayUntil()` 기반 주기 실행
8. LED ON/OFF/AUTO
9. B1 USER 버튼 입력
10. UART 명령·응답·이벤트·주기 상태 출력
11. Queue/Heap/Stack 상태 조회
12. 오류 통계와 복구 처리

최소한 Command Task, Control Task, Monitor Task, UART TX Task의 역할을 분리한다.

버튼 Notification을 안정적으로 처리하기 위해 별도 Button Task가 필요하다면 추가해도 된다.

권장 구조:

- **Command Task:** UART RX, Line Parsing, 명령 검증, Control Queue 전달
- **Control Task:** LED 상태, AUTO 모드, Queue 명령 처리, 상태 변경
- **Button Task:** ISR Notification 수신, Debounce, Control Task로 이벤트 전달
- **Monitor Task:** 1초 주기 상태 수집 및 진단
- **UART TX Task:** UART 출력 단독 소유

UART RX에 인터럽트를 사용한다면 ISR에서 Command Task로 Notification을 보내는 구조도 허용한다.

태스크 개수와 우선순위는 실제 구조에 맞게 결정하고 README에 근거를 문서화한다.

Idle Task를 포함한 전체 Task 수와 애플리케이션 Task 수를 구분하라.

## 4. 필수 FreeRTOS API

다음 API가 실제로 사용되도록 구현한다.

- `xTaskCreate()`
- `vTaskStartScheduler()`
- `vTaskDelayUntil()`
- `xQueueCreate()`
- `xQueueSend()`
- `xQueueReceive()`
- `xTaskNotifyFromISR()` 또는 동등한 FromISR API
- `xTaskNotifyWait()` 또는 동등한 수신 API
- `xSemaphoreCreateMutex()`
- `xSemaphoreTake()`
- `xSemaphoreGive()`
- `uxTaskGetStackHighWaterMark()`
- `xPortGetFreeHeapSize()`

Task 상태 조회를 위해 필요한 FreeRTOS 설정과 API도 추가한다.

각 API에 대해 실제 사용 목적을 설명한다.

특히 다음을 구분하라.

- Task가 BLOCKED인 정상 상태
- Task가 READY이지만 아직 실행되지 않은 상태
- 주기 실행을 위한 Delay
- Queue나 Notification 이벤트를 기다리는 Blocking
- 실제 오류 또는 비정상 정지

Task가 BLOCKED라는 이유만으로 Fault로 표시하지 않는다.

## 5. 시간 제어 — 최우선 검증 항목

**시간은 명령 실행 순서가 아니라 실제 경과시간을 기준으로 처리해야 한다.**

이전 프로젝트에서 30초로 설계된 공정이 실제 시간 지연 없이 매우 짧은 시간에 완료된 문제가 있었다.

이번 프로젝트에서는 반드시 이런 오류를 방지한다.

### 5.1 FreeRTOS Tick

기본 Tick 설정은 1000Hz, 즉 1ms로 한다.

단, 실제 FreeRTOS Tick과 물리적 경과시간이 일치하는지 검증한다.

`pdMS_TO_TICKS()`를 사용하고, Tick 변환 결과와 정수 오버플로 및 Tick Wraparound를 고려한다.

### 5.2 Monitor 주기

Monitor Task는 `vTaskDelayUntil()`을 이용해 1000ms마다 STATUS를 생성한다.

기대 동작:

- 실제 약 1초 경과 → STATUS 1회
- 실제 약 5초 경과 → STATUS 약 5회
- 실제 약 10초 경과 → STATUS 약 10회

출력되는 Uptime 값만으로 PASS 처리하지 않는다.

### 5.3 LED AUTO

기본 LED AUTO 주기는 **500ms마다 상태 토글**이다.

예시:

| 실제 경과시간 | LED |
| --- | --- |
| 0ms | OFF |
| 500ms | ON |
| 1000ms | OFF |
| 1500ms | ON |
| 2000ms | OFF |

AUTO 진입 시 LED 상태와 첫 토글 시점은 코드에서 명확하게 정의한다.

명령:

- `led on`
- `led off`
- `led auto`
- `rate 100`
- `rate 500`
- `rate 1000`
- `rate 5000`

`rate N`은 100\~5000ms 범위로 제한한다.

범위를 벗어난 입력은 ERR 응답을 반환한다.

AUTO 모드는 실제 시간 경과에 따라 LED를 전환해야 한다.

`next_toggle = now + period`를 무조건 반복하여 지연이 계속 누적되지 않도록 설계한다.

고정된 목표 시점을 기준으로 다음 Deadline을 계산하고, Task 실행이 늦어졌을 때의 정책을 명확하게 정의한다.

### 5.4 Button Debounce

실제 B1 버튼 입력은 50ms Debounce를 적용한다.

ISR에서는 최소한의 작업만 수행하고, Debounce와 이벤트 처리는 Task Context에서 수행한다.

한 번의 물리적 버튼 누름이 여러 번의 입력으로 잘못 기록되지 않도록 한다.

버튼을 계속 누르고 있을 때의 반복 입력 정책도 정의한다.

### 5.5 실제 시간 측정

Windows Host Monitor는 Windows의 Monotonic Clock, 예를 들어 `QueryPerformanceCounter()`를 사용한다.

다음을 측정한다.

- MCU Uptime 변화량
- PC 실제 경과시간
- STATUS 수신 간격
- LED EVT 발생 간격
- 목표 주기와 관찰 주기의 차이
- 주기 편차 및 누적 오차

예시:

```
[TIMING] host_elapsed_ms=10012
[TIMING] mcu_elapsed_ms=10000
[TIMING] drift_ms=-12
[TIMING] status_interval_avg_ms=1000.4
```

위 수치는 출력 형식 예시이며 실제 측정 결과가 아니다.

10초 이상 관찰하는 Timing Test를 제공한다.

Host 수신 시각에는 UART 전송 및 Windows 스케줄링 지연이 포함될 수 있으므로, MCU 이벤트 타임스탬프와 Host 수신 타임스탬프를 구분한다.

허용 오차 기준을 합리적으로 정하고, 실제 측정값과 함께 보고한다.

Host 테스트만으로 실제 하드웨어 시간 정확성을 검증했다고 주장하지 마라.

## 6. STM32 인터럽트 및 HAL 안전성

다음을 반드시 확인한다.

- Cortex-M4F용 FreeRTOS Port
- SysTick과 HAL Timebase 충돌 방지
- SVC/PendSV/SysTick 핸들러 중복 정의 방지
- NVIC Priority Grouping
- `configMAX_SYSCALL_INTERRUPT_PRIORITY`
- ISR에서 FreeRTOS API 호출 가능한 우선순위
- ISR에서 FromISR API 사용
- ISR에서 Blocking API 호출 금지
- UART RX Overrun/Overflow
- Task Stack Overflow
- Heap Allocation Failure
- `configASSERT`
- Priority Inversion 및 Mutex 사용
- UART 출력으로 인한 Task Starvation 방지

STM32F411RE에 존재하는 타이머만 사용한다.

HAL Timebase 분리가 필요하다면 TIM5를 사용하는 방안을 검토한다.

단, TIM5와 다른 시간 계측 기능의 중복 사용이 발생하지 않도록 한다.

기존 Equipment Controller의 인터럽트 설정을 무조건 복사하지 마라.

FreeRTOS IRQ Priority가 실제 STM32 NVIC 설정과 호환되는지 검증한다.

Task가 정지하거나 MCU가 HardFault에 진입해도 자동으로 Fail-safe가 보장된다고 주장하지 마라.

Watchdog은 이번 범위에서 제외하고 한계를 문서화한다.

## 7. UART Protocol

LF 종료 ASCII Line Protocol을 사용한다.

Host → STM32:

```
CMD 12 STATUS
CMD 13 LED ON
CMD 14 TASKS
CMD 15 RATE 500
```

STM32 → Host:

```
RSP 12 OK uptime_ms=12000 led=OFF
RSP 13 OK
RSP 14 OK tasks=6
RSP 15 OK rate_ms=500
EVT button pressed count=3 uptime_ms=12500
EVT led OFF ON uptime_ms=13000
STAT uptime_ms=13000 led=ON tasks=6 queue=0
```

필수 조건:

- 모든 CMD에 요청 ID 포함
- RSP에 동일 요청 ID 포함
- EVT는 비동기 이벤트
- STAT는 1초 주기 상태
- 명령 오류는 ERR 반환
- 최대 Line Length 정의
- 부분 수신 처리
- Overlong Line 안전 폐기
- 다음 LF 이후 Parser 복구
- Queue Full 및 Drop Count 기록
- UART TX는 단일 Task에서 수행
- 느린 Host 때문에 제어 Task가 무기한 차단되지 않도록 구성

`tasks`와 같이 결과가 길어지는 명령은 여러 응답 프레임을 사용해도 된다.

그 경우 요청 ID와 응답 완료 기준을 명확하게 정의하여 Host가 해당 CLI에 정확히 전달하도록 한다.

응답과 이벤트가 섞이더라도 명령 결과가 다른 CLI로 전달되지 않아야 한다.

출력 큐 포화 시 RSP, EVT, STAT의 우선순위 및 유실 정책을 정의한다.

## 8. Windows Host Monitor 및 CLI — 모두 C11

**Python을 사용하지 마라.**

Host 프로그램은 C11로 작성한다.

Windows API와 Winsock2 사용을 허용한다.

### Terminal 1 — Monitor

역할:

- COM7 단독 점유
- UART RX 수신
- Protocol Parser
- STATUS 표시
- EVT 즉시 표시
- CLI 명령 전달
- RSP를 해당 CLI로 라우팅
- Timing Diagnostics
- localhost TCP 서버

기본 포트는 19006이며 바인딩 주소는 127.0.0.1로 제한한다.

### Terminal 2 — CLI

역할:

- Monitor의 localhost TCP 서버에 접속
- 사용자 명령 입력
- 요청 ID 관리
- 명령 결과 표시
- Timeout/Disconnect 처리
- CLI 종료 및 재접속

최소 명령:

| 명령 | 기능 |
| --- | --- |
| help | 명령 목록 |
| ping | 통신 확인 |
| status | 시스템 상태 |
| tasks | Task 상태·우선순위·Stack |
| queue | Queue Depth·Drop |
| led on | LD2 ON |
| led off | LD2 OFF |
| led auto | 자동 토글 |
| rate N | LED 토글 주기 변경 |
| exit | CLI 종료 |

CLI를 종료해도 Monitor와 STM32는 계속 실행되어야 한다.

CLI 재접속이 가능해야 한다.

Monitor가 종료되면 CLI는 연결 종료를 감지해야 한다.

## 9. WSL 중심 개발·빌드·실행

모든 개발 작업은 WSL2 Ubuntu에서 수행한다.

Windows PowerShell에서 따로 개발하거나 빌드하는 것을 기본 절차로 만들지 마라.

### 9.1 STM32 펌웨어

WSL에서 `arm-none-eabi-gcc`로 빌드한다.

독립 CMake 빌드를 구성한다.

가능하면 다음과 같은 명령을 제공한다.

```
cd ~/projects/stm32-freertos-lab
cmake --preset Debug
cmake --build --preset Debug
```

ELF, BIN 및 Linker Map을 생성한다.

### 9.2 Host Monitor/CLI

WSL에 MinGW-w64를 사용하여 Windows용 실행 파일을 크로스 컴파일한다.

예상 산출물:

- `monitor.exe`
- `cli.exe`

`x86_64-w64-mingw32-gcc`와 필요한 Winsock2 라이브러리를 사용한다.

Host 테스트는 WSL의 Linux GCC로도 실행할 수 있도록 플랫폼 독립적인 Protocol/Timing 코드를 분리한다.

### 9.3 실제 실행

WSL 터미널 두 개에서 Windows `.exe`를 WSL Interop으로 실행한다.

Terminal 1:

```
./monitor.exe --serial COM7 --baud 115200
```

Terminal 2:

```
./cli.exe --host 127.0.0.1 --port 19006
```

위 명령은 예상 실행 형태다. 실제 빌드 경로를 반영하여 명령을 README에 작성하라.

WSL Interop 실행 가능 여부를 실제 확인하라.

필요하다면 Windows에서 접근 가능한 별도 빌드 산출물 디렉터리에 `.exe`를 배치하고 WSL에서 실행하는 방안을 사용한다.

단, 기존 Equipment Controller는 수정하지 않는다.

WSL에서 실행되는 Windows `.exe`는 Windows COM7에 접근하도록 한다.

WSL의 `/dev/ttyACM0`을 COM7이라고 가정하지 마라.

WSL Interop이 사용할 수 없다면 환경상의 제한을 보고하고 대안을 제시하되, Windows GUI 프로그램이나 별도 Bridge 터미널을 요구하는 설계로 무단 변경하지 마라.

## 10. 상태 및 통계

Monitor와 CLI에서 가능한 범위의 다음 정보를 관찰할 수 있어야 한다.

- Uptime
- LED 상태 및 AUTO 주기
- Button Event Count
- Application Task Count
- 전체 FreeRTOS Task Count
- Task Name
- Task Priority
- Task State
- Stack High-Water Mark
- Free Heap
- Minimum Ever Free Heap
- Control Queue Depth
- TX Queue Depth
- Queue Drop Count
- UART RX Overflow/Overrun Count
- Protocol Error Count
- Timing Diagnostics

수집 비용이 큰 통계는 `tasks`, `queue`, `status` 명령으로 조회하도록 해도 된다.

공유 상태는 Mutex 등으로 보호한다.

Mutex를 보유한 동안 긴 UART 전송을 수행하지 않는다.

## 11. 테스트 요구사항

### Test 01 — Scheduler

- Scheduler 시작
- 모든 Application Task 생성
- Task Priority 관찰
- Task 상태 조회
- 1초 STATUS 출력

### Test 02 — LED

- `led on`
- `led off`
- `led auto`
- `rate 500`
- `rate 1000`
- `rate 50` 오류 처리

실제 LED 토글 시간 측정 기능 포함.

### Test 03 — Queue

- CLI 명령 입력
- Command Task 수신
- Control Queue 전달
- Control Task 처리
- CLI RSP 반환
- Monitor EVT 출력

Queue Full 상황과 복구를 검증한다.

### Test 04 — ISR Notification

- B1 버튼 누름
- GPIO EXTI 발생
- ISR Notification 전달
- Task Context에서 처리
- Button Event Count 증가
- Monitor 이벤트 표시

Debounce 검증 포함.

### Test 05 — Mutex

- 공유 상태 접근
- Mutex 사용
- 상태 일관성 확인
- Lock 범위 확인

### Test 06 — Scheduling/Timing

- `vTaskDelayUntil()` 주기 확인
- STATUS 1초 주기
- LED 500ms 토글
- 실제 10초 경과 측정
- MCU와 Host 시간 비교
- 부하 상황에서 주기 편차 관찰

### Test 07 — UART/Protocol

- 정상 명령
- 잘못된 명령
- 잘못된 요청 ID
- 부분 수신
- 긴 입력
- 버퍼 Overflow
- Queue Full
- UART Overrun
- 복구 후 정상 명령 처리

### Test 08 — Host Bridge

- CLI 접속
- CLI 재접속
- CLI 종료 후 Monitor 지속
- Monitor 종료 시 CLI 연결 해제
- COM 연결 실패
- 요청 ID별 응답 라우팅
- 여러 CLI가 접속한 상황
- 느린 Host 상황

### Test 09 — Restart

- STM32 재부팅 후 초기 상태 확인
- Host 재접속
- 초기 LED 상태 확인
- 비정상 종료 후 재실행

## 12. 검증 결과 분류

반드시 다음을 구분한다.

**BUILD VERIFIED**

- ARM GCC 컴파일 및 링크 성공
- ELF/BIN 생성
- Linker Map 확인
- 메모리 사용량 확인

**HOST VERIFIED**

- C Host Test 통과
- Protocol Test 통과
- Mock UART Test 통과
- Windows Host 빌드 성공 시 별도 기록
- 실제 Windows Host 실행 검증 시 별도 기록
- Timing 계산 및 Host 경과시간 테스트

**HW VERIFIED**

실제 NUCLEO-F411RE에서 사용자가 Flash 및 동작을 확인한 경우에만 사용한다.

검증 대상:

- Scheduler
- Task 실행
- USART2 COM7 통신
- LD2 LED
- B1 버튼
- Queue 및 Notification
- 실제 1초 STATUS
- 실제 LED 토글 주기
- 오류 후 복구

실측하지 않은 성공률, 지연시간, 오차값은 생성하지 마라.

## 13. Flash 관련 제약

자동 Flash를 절대 수행하지 마라.

펌웨어 빌드 완료 후 Flash에 사용할 BIN 파일의 정확한 경로와 수동 명령만 제시한다.

사용자가 실장비 연결을 확인하고 직접 Flash한다.

Flash 시 기존 Equipment Controller 펌웨어가 보드에서 교체되므로 이 점을 명시한다.

실물 장비 연결 이전에는 하드웨어 동작 완료를 주장하지 않는다.

## 14. 산출물

다음을 제공한다.

1. 독립 FreeRTOS Firmware
2. ARM GCC CMake 빌드 구성
3. 버전 고정 FreeRTOS/HAL/CMSIS 의존성
4. C11 Windows Monitor
5. C11 Windows CLI
6. C Host Protocol/Timing 테스트
7. UART Protocol 명세
8. Task Priority/Queue Ownership 설계표
9. FreeRTOS API 사용 설명
10. WSL 빌드 절차
11. WSL 터미널 2개 실행 절차
12. STM32 수동 Flash 절차
13. 실제 하드웨어 검증 체크리스트
14. 빌드 및 테스트 결과
15. Known Limitations

README는 실제 구현 및 검증 상태와 일치하도록 수정한다.

## 15. 완료 기준

- 독립 프로젝트 구성
- 기존 Equipment Controller 무변경
- FreeRTOS Cortex-M4F ARM GCC 빌드
- Scheduler 및 다중 Task
- Queue 명령 전달
- Mutex 공유 상태 보호
- FromISR Task Notification
- 1초 주기 STATUS 구현
- LED ON/OFF/AUTO 구현
- LED 시간 기반 토글 구현
- B1 버튼 이벤트 구현
- C11 Host Monitor/CLI
- Windows COM7 UART 연결 구조
- WSL 중심 빌드 및 실행
- Task/Queue/Heap/Stack 조회
- MCU/Host 시간 비교 기능
- Host 테스트 실행
- 실제 빌드 및 테스트 결과 보고
- 수동 Flash 및 HW 검증 절차 문서화

실제 보드 검증이 필요한 항목은 사용자의 하드웨어 테스트 이후 완료 처리한다.

## 16. 작업 방식 및 지시

다음 순서대로 진행하라.

**Phase 1:** 기존 파일 및 의존성 분석

**Phase 2:** STM32 Firmware 수정 및 구현

**Phase 3:** C Host Monitor/CLI 구현 점검

**Phase 4:** 시간 제어 및 Timing Test 보완

**Phase 5:** ARM GCC Build, Host Test, WSL Interop/Windows 실행 검증

**Phase 6:** README 수정 및 결과 보고

진행 중 실제 컴파일 오류가 있다면 수정한 뒤 다시 빌드하라.

일부 검증을 수행할 수 없더라도 가능한 작업은 끝까지 수행하고, 미검증 항목은 명확히 남겨라.

**절대 금지:**

- Git Commit
- Git Push
- 사용자 승인 없는 Flash
- 기존 Equipment Controller 수정
- 불필요한 대규모 리팩터링
- Python으로 Host 프로그램 변경
- HW 실측 없이 HW VERIFIED 주장
- 테스트 미실행 상태에서 PASS 주장

보고 형식:

1. 구현 및 수정 완료 사항
2. 변경·추가 파일 목록
3. 실제 수행한 빌드 명령과 결과
4. 실제 수행한 테스트와 PASS/FAIL
5. Flash/RAM 사용량
6. Timing 검증 결과
7. 미검증 항목
8. Known Limitations
9. 사용자 수동 Flash 명령
10. 실장비에서 수행할 테스트 순서

**단순 검토나 설계 제안으로 종료하지 말고, 실제 소스 수정·빌드·테스트까지 가능한 범위에서 완료하라. 불필요한 중간 질문 없이 작업을 진행하라.**

목표는 `STM32F411RE + FreeRTOS + C Monitor/CLI`가 실제 시간과 태스크 동작을 관찰할 수 있는 하나의 재현 가능한 독립 프로젝트로 완성되는 것이다.
