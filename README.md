# STM32 FreeRTOS Core SW Lab

NUCLEO-F411RE(STM32F411RET6), C11, FreeRTOS 실습. 개발·빌드·Windows 실행의 시작점은 WSL2입니다. Equipment Controller 소스/빌드에는 의존하지 않습니다. 자동 Flash, Git commit/push는 수행하지 않습니다.

**2026-10-29 검증:** ARM ELF/BIN/MAP 생성 **BUILD VERIFIED**. Linux 테스트 3/3, Windows 테스트 4/4 및 실제 EXE 실행 **HOST VERIFIED**. Windows COM7 열기/기존 펌웨어 데이터 수신 확인. **새 펌웨어 HW VERIFIED는 아직 없음.** 현재 보드에서는 `DOOR:OPEN`, `ESTOP:OFF` 같은 기존 펌웨어 출력이 관찰되었습니다. 새 RTOS 이미지가 실행되고 있다는 증거가 아닙니다.

## 빌드: WSL

필수 도구: CMake >=3.22, Ninja, Git, GCC, ARM GCC/newlib, MinGW-w64. Ubuntu 설치 예:

```bash
sudo apt-get update
sudo apt-get install cmake ninja-build git gcc gcc-arm-none-eabi libnewlib-arm-none-eabi gcc-mingw-w64-x86-64
cd ~/projects/stm32-freertos-lab
cmake --preset Debug
cmake --build --preset Debug
arm-none-eabi-size build/Debug/stm32-freertos-lab.elf
./scripts/build-windows.sh
cmake -S host -B build/host-linux -G Ninja
cmake --build build/host-linux
ctest --test-dir build/host-linux --output-on-failure
ctest --test-dir build/host-win --output-on-failure
```

Windows 테스트는 WSL Interop이 켜져 있어야 실행됩니다. 본 환경에서 EXE 직접 실행과 CTest 실행을 확인했습니다. `build-windows.sh`는 MinGW가 없으면 Ubuntu 패키지를 `build/toolchain-debs/`에 다운로드하고 `build/mingw/`에 풀어 사용합니다. sudo 없이 동작하며 시스템 설치를 변경하지 않습니다. apt 다운로드 실패는 즉시 종료됩니다. 일반 MinGW 설치 시 직접 빌드도 가능합니다:

```bash
cmake -S host -B build/host-win -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mingw-w64.cmake"
cmake --build build/host-win
```

컴파일러 경로가 달라지면 해당 Host 빌드 디렉터리를 새로 지정하세요. 이동/Clone 시 `build/`는 복사하지 말고 재생성합니다.

| 공식 의존성 | 고정 커밋 |
|---|---|
| ARM CMSIS 5.9.0 | `2b7495b8535bdcb306dac29b9ded4cfb679d7e5c` |
| ST CMSIS Device F4 v2.6.9 | `3b64b7b0b546cefe84f13c65508126eea58b97d8` |
| STM32F4 HAL v1.8.3 | `c2e1406d7ea4b73aa42b98ddeb75a8670b1a5a16` |
| FreeRTOS Kernel V11.3.1 | `3a22924e0a9ddbbc8b0758881c33b3422a5cc20d` |

CMake FetchContent가 공식 GitHub 저장소에서 다운로드합니다. configure에서 실제 HEAD 및 tracked 수정 여부를 검사하며, 로컬 override도 같은 핀을 요구합니다. 최초 빌드에는 인터넷이 필요합니다. 다운로드 실패 시 configure가 실패하므로 오류 로그의 저장소/네트워크 원인을 해결 후 재시도하세요. 잘못된 핀을 넣는 negative test에서 실제 configure 실패를 확인했습니다. Equipment Controller의 HAL/CMSIS 경로를 사용하지 않습니다.

## WSL 터미널 두 개에서 실행

Terminal 1:

```bash
cd ~/projects/stm32-freertos-lab
./build/host-win/monitor.exe --serial COM7 --baud 115200 --port 19006
```

Terminal 2:

```bash
cd ~/projects/stm32-freertos-lab
./build/host-win/cli.exe --host 127.0.0.1 --port 19006
```

Windows EXE가 **Windows COM7**을 엽니다. `/dev/ttyACM0`과 연결하지 않습니다. Monitor만 COM을 독점하고 TCP 서버는 `127.0.0.1`에만 바인딩합니다. CLI는 최대 8개 동시 접속 가능하며 `exit` 후 재접속할 수 있습니다. Monitor가 종료되면 입력 대기 중인 CLI도 종료됩니다. Monitor 종료는 Ctrl+C입니다.

Interop 문제 발생 시 `/proc/sys/fs/binfmt_misc/WSLInterop`과 `/mnt/c/Windows/System32/cmd.exe /c ver`를 확인합니다. UNC 실행이 제한된 환경에서는 두 EXE를 `/mnt/c/Users/<사용자>/stm32-lab/`에 복사하고 **WSL에서** 그 경로의 EXE를 실행할 수 있습니다. Interop 자체가 비활성이면 WSL 설정 복구가 필요합니다. Python, GUI UART 프로그램, 별도 Bridge 터미널은 필요하지 않습니다.

| CLI | 기능 |
|---|---|
| `help`, `ping` | 목록, PONG |
| `status` | uptime, LED/mode/rate, 버튼, task 수, heap/minheap, protocol/UART/TX/drop 통계, 버튼 notification 수 |
| `tasks` | Command/Control/Monitor/UartTx/Idle 상태·우선순위·Stack watermark |
| `queue` | control/RSP/log 깊이·용량·drop |
| `led on`, `led off`, `led auto` | LD2 제어 |
| `rate N` | 100~5000ms, 기본 500ms; 범위 밖은 ERR |
| `exit` | 해당 CLI 종료 |

## Task 및 소유권

Application Task **4개**, Idle 포함 **총 5개**. Timer daemon은 비활성화되어 있습니다.

| Task (실제 이름) | 우선순위 | Stack(words) | 소유/역할 | 정상 Blocking |
|---|---:|---:|---|---|
| Command | 4 | 512 | RX ring 소비, line parser, query, control queue 생산 | RX notification 최대 50ms; 64byte마다 1ms 양보 |
| Control | 3 | 512 | LED/GPIO 출력, AUTO deadline, 버튼 debounce, LabState 유일 writer | control queue 최대 10ms, TX enqueue 최대 20ms |
| Monitor | 2 | 512 | 1초 STAT 생성 | `vTaskDelayUntil(1000ms)` |
| UartTx | 1 | 512 | USART2 송신 단독 소유 | log queue 최대 5ms, UART timeout 40ms |
| IDLE | 0 | 128 | 커널 유휴 처리 | 커널 관리 |

Monitor를 polling UART TX보다 높게 두어 UART backlog가 1초 진단 Task를 굶기지 않도록 했습니다. Control/Command는 긴 UART 전송을 하지 않습니다. Mutex priority inheritance가 켜져 있고 LabState 복사/갱신에만 잠금을 사용합니다. 디버그 통계는 각 시점의 관측치이며 여러 Task 전체의 원자적 snapshot은 아닙니다.

| 객체 | 용량 | 생산자 → 소비자 | 포화 정책 |
|---|---:|---|---|
| RX ring | 255 유효 byte | USART2 ISR → Command | overflow/error 카운트, unread backlog 폐기, 실제 다음 LF까지 재동기화 |
| control queue | 8 | Command → Control | 대기 없이 ERR control_queue_full, cmddrop++ |
| RSP queue | 8 × 320byte | Command/Control → UartTx | 최대 20ms 대기 후 rspdrop++; Host 5초 timeout |
| log queue | 12 × 320byte | Control/Monitor → UartTx | EVT/STAT 동등 FIFO, 대기 없이 logdrop++ |
| state mutex | 1 | Control writer, Command/Monitor reader | 짧은 복사/갱신만; UART 출력 중 보유 금지 |

RSP가 log보다 우선합니다. 이미 시작한 log 전송은 선점하지 않으며 log 대기는 최대 5ms입니다. 응답 보장은 무한하지 않습니다. 혼잡 시 RSP도 드롭될 수 있으므로 새 요청으로 재시도하며, timeout된 제어 명령이 실행되었을 가능성도 고려해야 합니다.

`tasks` 상태: X=RUNNING, R=READY(실행 자격이 있지만 CPU 대기), B=BLOCKED(Queue/Notification/Delay 대기), S=SUSPENDED, D=DELETED. **B는 정상 상태이며 fault 표시가 아닙니다.** Command 조회 중 Command가 X로 보이는 것은 정상입니다. Stack watermark는 최소 남은 **32-bit words**이며 bytes로 보려면 4를 곱합니다.

## FreeRTOS API 실제 사용

| API | 사용 목적 |
|---|---|
| `xTaskCreate`, `vTaskStartScheduler` | 4개 application task 생성 및 scheduler 시작 |
| `vTaskDelayUntil` | Monitor의 고정 1초 release |
| `xQueueCreate`, `xQueueSend`, `xQueueReceive` | control 명령과 두 TX lane |
| `vTaskNotifyGiveFromISR`, `ulTaskNotifyTake` | UART byte/버튼 edge ISR 알림 → Task 수신; 알림은 합쳐질 수 있어 ring/현재 GPIO level로 처리 |
| `xSemaphoreCreateMutex`, `xSemaphoreTake`, `xSemaphoreGive` | LabState 보호, priority inheritance |
| `uxTaskGetStackHighWaterMark` | tasks 응답, Stack 최소 잔여량 |
| `xPortGetFreeHeapSize`, `xPortGetMinimumEverFreeHeapSize` | 현재/최소 잔여 RTOS heap |
| `eTaskGetState`, `uxTaskPriorityGet`, `uxTaskGetNumberOfTasks`, `xTaskGetIdleTaskHandle` | Idle 포함 task 관찰 |

## 시간 및 인터럽트

- Cortex-M4F GCC port, FPU hard-float. HSI 16MHz → PLL 84MHz. SysTick 1000Hz, 32-bit tick. 모든 delay/rate는 `pdMS_TO_TICKS` 사용. tick=ms인 설정을 전제로 `lab_time.h`를 사용합니다.
- 초기 LED는 OFF. `led auto`는 **즉시 OFF로 초기화**하고 한 period 뒤 처음 ON. `rate`는 현재 시점에서 새 period를 적용해 deadline을 다시 시작합니다.
- AUTO는 `deadline += (skipped+1)*period` 방식. 늦으면 실제 토글 1회만 하고 지나간 slot을 `missed`로 셉니다. 빠른 catch-up 토글을 만들지 않고 원래 phase를 유지합니다. 명령에 의한 재설정 외에 매번 `now+period`로 늦어지지 않습니다.
- Monitor는 1초 고정 release, 전체 주기를 놓친 경우 지난 release를 건너뜁니다. 누락된 시간을 가짜 STAT burst로 채우지 않습니다. UART 큐 때문에 생성/수신 시각은 달라질 수 있습니다.
- uint32 차이로 wraparound를 처리합니다. deadline 비교는 반 주기(약 24.8일) 미만 간격을 전제로 하며 uptime은 약 49.7일에 wrap됩니다. Host는 wrap을 구분하고 뒤로 이동하는 uptime을 reset/discontinuity로 취급합니다.
- B1 PC13 active-low, 양 edge EXTI. ST [공식 Nucleo BSP](https://github.com/STMicroelectronics/stm32f4xx-nucleo-bsp/blob/main/stm32f4xx_nucleo.c)의 falling press 설정을 참고했습니다. ISR은 알림만 보내고 Control이 실제 level의 50ms 안정화를 검사합니다. 길게 눌러도 1회, **50ms 안정된 해제 이후** 재무장합니다. 샘플 간격/스케줄링에 따라 50ms보다 늦게 확정될 수 있습니다.
- FreeRTOS가 SysTick/SVC/PendSV를 소유합니다. HAL 1ms timebase는 **TIM5만** 사용하고 별도 측정 타이머로 중복 사용하지 않습니다. ELF에서 각 handler strong symbol 1개 확인.
- NVIC group4, 4 priority bits. MAX_SYSCALL=5 (`0x50`), USART2/EXTI/TIM5=6, PendSV/SysTick=15. FromISR는 API 허용 우선순위에서만 호출하며 ISR에 blocking API가 없습니다.
- `configASSERT`, malloc-failed hook, Stack overflow mode2는 인터럽트를 막고 정지합니다. watchdog은 없습니다. HardFault/Task 정지 시 LED OFF나 자동 복구를 보장하지 않습니다.

## UART/TCP protocol

115200 8N1, PA2 TX/PA3 RX, LF 종료 ASCII. CRLF도 허용. UART 입력 최대 **127자(LF 제외)**, 출력 최대 **318자 + LF**(320byte 저장, NUL 포함), Host parser 최대 512자. 부분 수신을 누적합니다. 잘못된 byte/긴 line은 다음 LF까지 버리며 이후 정상 명령을 처리합니다.

```text
CMD 12 STATUS
CMD 13 LED ON
CMD 14 TASKS
CMD 15 RATE 500
RSP 12 OK uptime_ms=... led=... app_tasks=4 tasks=5 ...
RSP 13 OK rate_ms=500
RSP 14 OK tasks=5 cmd=X,4,... ctrl=B,3,... idle=R,0,... unit=words
RSP 15 OK rate_ms=500
EVT button pressed count=3 uptime_ms=12500
EVT led OFF ON mode=AUTO rate_ms=500 uptime_ms=13000
EVT config mode=AUTO rate_ms=500 uptime_ms=13000
STAT uptime_ms=13000 led=ON mode=AUTO ...
```

숫자/상태는 형식 예시이며 측정 결과가 아닙니다. UART ID는 1..2147483647. 해석 가능한 ID에는 동일 ID로 OK/ERR, 해석 불가능한 frame/ID에는 `RSP 0 ERR ...`. 모든 정상 query는 **RSP 한 line로 완료**합니다. EVT/STAT는 명령 완료가 아닙니다.

CLI도 TCP에서 `CMD <local-id> <command>`를 보냅니다. Monitor는 클라이언트별 local ID를 UART global ID로 바꾸고 RSP를 해당 client의 local ID로 복원합니다. 같은 local ID를 쓰는 두 CLI가 섞이지 않습니다. client당 1개, 내부 요청 테이블 64개(현재 client 8개라 실제 동시 요청은 최대 8개). 요청은 5초 timeout, CLI 대기는 7초. 연결 해제 시 해당 route를 폐기하므로 늦은 RSP는 ORPHAN입니다. 비동기 boot 이벤트 수신 시 pending 요청을 mcu_reset 오류로 종료합니다. 느린 TCP client는 nonblocking send 실패 시 분리하고 다른 client를 유지합니다.

## Timing diagnostics와 10초 테스트

```bash
# 실제 Host 단조 시계 테스트. MCU 필드는 합성 데이터이며 보드 검증이 아님.
./build/host-linux/timing_tests
./build/host-win/timing_tests.exe

# Flash 이후 실제 보드 관찰. 실행 중 다른 WSL CLI에서 led auto/rate 500.
./build/host-win/monitor.exe --serial COM7 --baud 115200 --duration-ms 15000 | tee build/hw-timing.log
```

Windows는 `QueryPerformanceCounter`, Linux 테스트는 `CLOCK_MONOTONIC`을 사용합니다. `[STATUS host_ms=...]`/`[EVENT host_ms=...]`는 **PC 수신 시각**, `uptime_ms`는 **MCU 생성 시각**입니다. `[TIMING STATUS/LED]`는 host_elapsed, mcu_elapsed, drift=MCU−Host, interval 평균/min/max, 목표 대비 평균 및 누적 오차를 출력합니다. `window=10s_observed`는 관찰 기간 표시이며 자동 HW PASS가 아닙니다. `EVT config`에서 LED 측정 창을 초기화하므로 명령/주기 변경 전후를 섞지 않습니다.

실물 초기 합격 기준(온도 보정하지 않은 HSI와 UART/Windows 지연을 고려한 실습 기준):

- 같은 설정으로 **Host 수신 시각 기준 10초 이상**; 약 10개 STATUS interval.
- MCU−Host drift 절댓값 ≤ `max(300ms, host_elapsed×3%)`.
- 부하 없는 STATUS 수신 간격 평균 970~1030ms, 각 간격 800~1200ms.
- LED AUTO 평균 간격 오차 ≤ `max(30ms, period×3%)`; MCU EVT와 Host EVT 모두 기록. UART 출력만으로 전기적인 LED 파형 정확성을 인증하지 않음.
- `missed`, UART errors/drop이 0인지 함께 확인. 부하 테스트는 동일 기준의 만족 여부와 drop/missed를 별도로 기록하며 수신 유실이 있으면 정상 타이밍 PASS로 처리하지 않음.

기준 초과 시 먼저 drop, USB/Windows 지연, HSI 오차, Task 지연을 구분합니다. 측정값이 없으면 PASS를 기입하지 않습니다.

## 수동 Flash 및 하드웨어 체크리스트

**Flash하면 보드의 기존 Equipment Controller 펌웨어가 교체됩니다.** 사용자만 보드 연결을 확인하고 실행합니다. ST-LINK가 WSL에 전달되어 있고 `st-flash`가 설치된 경우:

```bash
st-flash --reset write /home/kccistc/projects/stm32-freertos-lab/build/Debug/stm32-freertos-lab.bin 0x08000000
```

Windows ST-LINK를 그대로 사용하려면 설치된 STM32CubeProgrammer CLI를 WSL에서 실행할 수도 있습니다(설치 경로 확인):

```bash
"/mnt/c/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe" \
  -c port=SWD -w "$(wslpath -w "$PWD/build/Debug/stm32-freertos-lab.bin")" 0x08000000 -v -rst
```

어느 Flash 명령도 이번 작업에서 실행하지 않았습니다. USB 장치를 WSL에 attach하면 Windows COM7 접근이 달라질 수 있으므로 Flash 이후 Windows ST-LINK VCP가 다시 COM7에 있는지 확인하세요.

| Test | 사용자가 보드에서 수행할 순서 | 현재 상태 |
|---|---|---|
| 01 Scheduler | Flash/reset → OFF → boot/scheduler EVT → 1초 STAT → tasks에서 4 app/5 total, priorities, 정상 B/R/X 관찰 | HW 미검증 |
| 02 LED | on/off 눈으로 확인 → auto → rate 500/1000/100/5000 → rate 50 ERR, 상태 불변 | HW 미검증 |
| 03 Queue | CLI 명령 RSP/EVT 확인; 여러 CLI로 반복 요청 → queue drop/depth 조회 → 부하 중지 후 ping 회복 | Host double queue full/recovery PASS, HW 미검증 |
| 04 ISR | 한 번 누름, 바운스, 3초 유지, 해제 후 재누름; count와 btnnotify 관찰 | Host debounce PASS, HW 미검증 |
| 05 Mutex | 여러 CLI status/tasks와 제어 동시 수행; 일관성/stack/heap 관찰 | 소스 및 Host double 확인, 실제 priority inheritance 미검증 |
| 06 Timing | 위 15초 관찰, 설정 유지 10초 이상 데이터; 부하 없이/있을 때 비교 | Host 시계만 검증, MCU 실측 없음 |
| 07 Protocol | 정상/오류/긴 입력/부분 전송/포화 후 ping; ORE 강제 주입은 별도 디버거 테스트 | Host parser/router/queue PASS, 실제 UART ORE 미검증 |
| 08 Bridge | CLI 2개, 종료/재접속, Monitor 종료, COM 단절, 느린 client | Windows mock/TCP 및 COM open/read 검증, 실물 단절 미검증 |
| 09 Restart | B2 reset → 초기 OFF/rate500/count0; boot 측정 창 reset, CLI 재접속; 비정상 종료 후 재실행 | 새 MCU 펌웨어 재시작 미검증 |

실측 결과를 `IMPLEMENTATION_REPORT.md`에 추가한 뒤 해당 항목만 HW VERIFIED로 변경하세요.

## 알려진 한계

- 새 펌웨어는 아직 Flash하지 않았으므로 실제 scheduler/LED/버튼/시간/오류 복구가 증명되지 않았습니다.
- Host firmware 테스트는 실제 `Dispatch`와 queue/mutex **test double**을 사용합니다. FreeRTOS scheduler, NVIC, UART 전기 신호를 시뮬레이션하지 않습니다.
- HSI 기반이며 온도에 따른 정밀 시간 보정은 없습니다. Host 수신에는 전송/버퍼/스케줄링 지연이 포함됩니다.
- EVT/STAT와 혼잡 시 RSP도 유실될 수 있습니다. 자동 재전송/영구 이력/정확히 한 번 실행 보장은 없습니다.
- HAL polling TX는 최대40ms timeout. Windows COM read/write는 각각20/200ms bound이며 느린 stdout도 Monitor를 지연시킬 수 있습니다. MCU 제어는 Host를 무한 대기하지 않습니다.
- `tasks`는 순간 관찰이라 READY나 RUNNING 포착을 항상 보장하지 않습니다. fault hook은 정지하고 복구하지 않습니다. watchdog/안전 인증/자동 fail-safe는 범위 밖입니다.
- reset boot EVT가 유실되면 pending 요청은 timeout으로 정리됩니다. 32bit uptime/reset 판별에는 반 wrap 범위 제한이 있습니다.
- 공식 HAL flash_ex에서 F411에 쓰이지 않는 Banks 인자 경고 3개가 최초 전체 빌드에 나타났습니다. 자체 소스 최종 빌드는 경고 없이 확인했습니다.

구체적인 실행 로그, 메모리 사용량, 변경 파일은 [IMPLEMENTATION_REPORT.md](IMPLEMENTATION_REPORT.md)를 참고하세요.
