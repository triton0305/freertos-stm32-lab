# 구현·검증 결과 — 2026-10-29 (Asia/Seoul)

## 1. 구현 및 수정 완료

기존 초안을 재사용하면서 실제 ARM compile 실패와 설계 결함을 수정했다.

- HAL UART의 `DMA_HandleTypeDef` 컴파일 오류: HAL DMA 설정/헤더 의존성 추가. UART DMA 동작을 새로 추가한 것은 아니다.
- B1 active-high 가정을 active-low로 수정, 양 edge 알림 + Task에서 50ms 안정화, 안정된 해제 전 재입력 금지.
- AUTO 진입 OFF/첫 deadline 명시, 절대 deadline 유지/늦은 slot 건너뛰기, wrap-safe 공유 시간 로직과 테스트.
- Monitor priority 2 > polling TX priority 1; Command RX 64byte마다 1ms 양보. Monitor 지연 시 과거 STATUS burst 방지.
- UART loss 시 unread backlog 및 다음 LF까지 폐기하는 복구 정책; HAL TX timeout 뒤 RX IRQ 재활성화.
- 최소 heap/protocol error/button notification 통계, Idle task 상태·watermark 추가. TX frame 저장 공간 320byte로 확장.
- CLI local ID → Monitor global UART ID → 원래 CLI ID 라우팅, wrap collision 회피, slow-client 정리, boot 시 pending request 종료.
- CLI 수신과 console 입력 분리: 입력 대기 중 Monitor 종료 감지. Win32 Interlocked를 통한 공유 ID 접근.
- Windows QPC 및 Linux monotonic 10초 테스트, interval 평균/min/max/목표 편차/누적 오차/MCU reset 관찰.
- 공식 의존성 4개 HEAD/수정 여부 검사, WSL MinGW toolchain과 sudo 없는 로컬 bootstrap script.

## 2. 변경·추가 파일

변경:

- `App/rtos_lab.c`
- `Core/board.c`, `Core/board.h`, `Core/FreeRTOSConfig.h`, `Core/stm32f4xx_hal_conf.h`
- `CMakeLists.txt`, `host/CMakeLists.txt`
- `tools/monitor.c`, `tools/cli.c`, `tools/protocol.c`
- `tests/test_host.c`
- `README.md`, `CODEX_HANDOFF.md`, `IMPLEMENTATION_REPORT.md`

추가:

- `App/lab_time.h`
- `cmake/mingw-w64.cmake`
- `scripts/build-windows.sh`
- `tools/timing.c`, `tools/timing.h`
- `tests/test_timing.c`, `tests/test_bridge_win.c`, `tests/test_firmware.c`
- `tests/fakes/FreeRTOS.h`, `tests/fakes/task.h`, `tests/fakes/queue.h`, `tests/fakes/semphr.h`

기존 linker script, ARM toolchain, preset 및 나머지 헤더는 검토 후 유지했다. 다운로드/산출물/로그는 `build/`에 있다. `cpp-equipment-controller`에는 쓰기 작업을 하지 않았다. 현재 Lab 디렉터리 자체에는 `.git`이 없었으며 commit/push는 수행하지 않았다.

## 3. 실제 빌드 명령 및 결과

```bash
cmake --preset Debug
cmake --build --preset Debug
arm-none-eabi-size build/Debug/stm32-freertos-lab.elf
./scripts/build-windows.sh
cmake -S host -B build/host-linux -G Ninja
cmake --build build/host-linux
```

- ARM GCC 10.3.1 configure/build/link 성공: **BUILD VERIFIED**.
- ELF/BIN/MAP 생성. `arm-none-eabi-nm --defined-only ...`로 SVC/PendSV/SysTick/TIM5 strong handler 각각 1개 확인.
- 공식 source 4개가 명세의 exact commit에 있는 것을 `git rev-parse HEAD`로 확인.
- MinGW compiler CMake 식별 GNU 10.0.0: Windows `monitor.exe`, `cli.exe`, 모든 테스트 EXE 빌드 성공.
- Linux GCC 11.4.0: Host 테스트 빌드 성공.
- 초기 ARM 빌드는 DMA 헤더 누락으로 FAIL했고 소스 수정 후 성공했다. 최초 전체 vendor HAL build에서 unused Banks 경고 3개. 자체 소스 최종 빌드 경고 없음.
- sudo apt 설치 시도는 비밀번호 요구로 실패했다. 사용자 디렉터리에 apt package를 다운로드/추출해 MinGW 크로스 빌드를 완료했다. 시스템 설치 변경 없음.
- 잘못된 `FREERTOS_KERNEL_SOURCE_DIR`를 다른 dependency로 지정한 negative configure: exit 1 및 expected/actual commit 메시지 확인 (**예상된 실패, 검증 PASS**).

로그: [최종 ARM](build/arm-final.log), [Windows build](build/windows-build.log), [Linux build](build/linux-build.log), [pin mismatch](build/pin-negative.log).

## 4. 실제 테스트와 PASS/FAIL

```bash
ctest --test-dir build/host-linux -V
ctest --test-dir build/host-win -V
./build/host-win/monitor.exe --mock --duration-ms 2500
printf 'ping\nstatus\nexit\n' | ./build/host-win/cli.exe
./build/host-win/monitor.exe --serial COM999 --duration-ms 500
./build/host-win/monitor.exe --serial COM7 --baud 115200 --duration-ms 1500
```

Mock Monitor와 CLI는 동시에 별도 process로 실행했다.

| 테스트 | 결과 | 범위 |
|---|---|---|
| Linux CTest | **3/3 PASS** | protocol, monotonic/deadline/debounce, firmware dispatch doubles |
| Windows CTest via WSL Interop | **4/4 PASS** | 위 3개 + 실제 Winsock TCP/mock UART bridge |
| Protocol | **29 checks PASS**, 양 플랫폼 | partial framing, invalid ID/byte, overflow/recovery, normalize, clock wrap/skew |
| Firmware dispatch | **20 checks PASS**, 양 플랫폼 | 실제 Dispatch 코드에 queue/mutex double 연결; control queue full/회복, RSP/log drops, rate 범위, 오류, 상태 |
| Windows bridge | **30 checks PASS** | 두 client 동일 local ID, 역순 응답, global ID wrap/collision, event 혼합, timeout, reset, long line, 재접속/늦은 RSP 격리, slow client 분리 |
| 실제 monitor.exe + cli.exe | **PASS** | mock UART PING/PONG, mock STATUS ERR, exit, Monitor 별도 유지 |
| idle CLI 종료 | **PASS** | FIFO 입력을 열린 채 둔 CLI가 duration Monitor 종료 후 5초 제한 전에 exit 0; 입력 없이 종료 |
| COM999 | **예상된 실패 PASS** | CreateFile 실패 Win32 error 2, process exit 1 |
| COM7 open/read | **PASS (Host 연결만)** | Windows COM 독점 open, 기존 펌웨어 ASCII 출력 수신, 자동 duration 종료 |
| 핀 불일치 | **예상된 실패 PASS** | configure가 다른 commit을 거부 |

**HOST VERIFIED**는 위 범위에만 사용한다. FreeRTOS scheduler/NVIC/UART ORE/물리 debounce를 Host double 테스트로 검증했다고 주장하지 않는다.

로그: [Linux CTest](build/host-linux-tests.log), [Windows CTest](build/host-win-tests.log), [실제 CLI](build/windows-cli-mock.log), [idle disconnect](build/cli-idle-disconnect.log), [COM 실패](build/windows-com-failure.log), [COM7](build/windows-com7.log).

COM7에서 관찰한 `DOOR:OPEN`, `ESTOP:OFF`, `PRESSURE:...` 등은 **기존 펌웨어 데이터**다. COM7 read 시험에서는 명령을 보내지 않았고 Flash하지 않았다. 새 Lab의 USART2 통신/HW VERIFIED로 분류하지 않는다.

## 5. Flash/RAM

최종 Debug 링크:

| 항목 | 사용량 | 총량 |
|---|---:|---:|
| FLASH (linker memory region) | **31,980 bytes (6.10%)** | 524,288 bytes |
| RAM (linker memory region) | **36,680 bytes (27.98%)** | 131,072 bytes |
| size text / data / bss | 31,796 / 144 / 36,544 bytes | — |
| BIN 크기 | **31,980 bytes** | — |

RAM region에는 RTOS heap 배열 32KiB 및 linker의 C heap/MSP reserve가 포함된다. Task stack/queue는 이 RTOS heap에서 동적 할당되므로 실제 free/minimum heap과 Task watermark는 보드 실행 후 조회해야 한다. 이를 아직 실측하지 않았다.

BIN SHA-256:

```text
03b4d53cc24589770142a632a39e6a4060f87de2615a0bdaa5d1d4d1256d15e6
```

## 6. Timing 결과

최종 CTest 로그의 **Host clock + 합성 MCU 데이터** 측정값:

| 플랫폼 | Host elapsed | 합성 MCU elapsed | MCU−Host drift | 평균 interval | min / max |
|---|---:|---:|---:|---:|---:|
| Linux CLOCK_MONOTONIC | 10000ms | 10000ms | 0ms | 1000.000ms | 1000 / 1000ms |
| Windows QPC | 10001ms | 10000ms | −1ms | 1000.100ms | 993 / 1014ms |

각 테스트가 해당 OS 단조 시계로 10초 이상 기다렸다. 테스트의 합성 MCU timestamp는 1000씩 증가시킨 입력 데이터이지 보드 시간 측정이 아니다. WSL CTest 자체의 wall-duration은 Windows QPC 측정과 다르게 나타날 수 있으므로 Windows 시험의 관찰 시간은 출력된 QPC delta로 기록한다. 실제 MCU drift/STATUS/LED 값은 **미측정**.

공유 deadline wrap, skipped slot, debounce 재무장, 긴 누름, reset/wrap 구분 테스트 PASS. Hardware acceptance 기준 및 15초 실행 명령은 README에 있으며, **수신 window 표시만으로 HW PASS하지 않는다**.

## 7. 미검증

- 새 이미지의 보드 Flash, scheduler 시작/priority 실행/실제 task 상태.
- LD2 on/off/auto 및 MCU 1000Hz tick의 물리 시간 일치.
- B1 EXTI/notification/50ms 물리 debounce.
- 실제 Cortex-M4F mutex priority inheritance, Stack/heap 실측 및 fault hook.
- UART ORE/ring overflow 실제 주입, 고부하 missed/drop 회복, USB 물리 분리/재연결.
- 새 펌웨어 reset 초기 상태 및 실제 boot 이벤트.

**HW VERIFIED: 없음.** 사용자의 수동 Flash/실장비 관찰 후에만 항목별 갱신한다.

## 8. Known Limitations

README의 한계를 적용한다. 특히 watchdog 없음, fault 시 LED 안전 상태 보장 없음, HSI 정밀도/Windows 수신 지연, 큐 포화 시 이벤트/응답 유실, timeout 명령의 실행 여부 불확실성이 있다. Host mock은 scheduler나 물리 UART를 대신하지 않는다. `--mock`은 PING 및 시험용 TIMEOUT만 구현한 테스트 transport로, 실제 보드 기능 시뮬레이터가 아니다.

## 9. 사용자 수동 Flash

**기존 Equipment Controller 펌웨어가 보드에서 교체된다.** 사용자가 연결과 대상 보드를 확인한 뒤 직접 실행:

```bash
st-flash --reset write /home/kccistc/projects/stm32-freertos-lab/build/Debug/stm32-freertos-lab.bin 0x08000000
```

WSL ST-LINK USB 전달/도구 설치가 필요하다. Windows CubeProgrammer CLI를 WSL에서 실행하는 대안은 README에 있다. 둘 다 자동 실행하지 않았다.

## 10. 실장비 테스트 순서

1. 사용자 Flash/reset → 초기 OFF, boot/scheduler 이벤트, 4 application/5 total tasks 확인.
2. WSL 두 터미널에서 Monitor(COM7) 및 CLI 실행 → ping/status/tasks/queue.
3. LED ON/OFF/AUTO, rate 100/500/1000/5000 및 rate50 ERR.
4. 설정을 고정한 15초 기록 → 10초 이상 STATUS/LED QPC↔MCU 비교 및 README 허용오차 판정.
5. B1 한 번/바운스/3초 유지/해제/재누름 → count 및 btnnotify 확인.
6. 두 CLI 및 부하 → queue/drop/stack/heap 조회, 부하 중단 후 정상 명령 회복.
7. invalid/partial/overlong 입력과 오류 회복, 필요시 디버거로 UART ORE 주입.
8. CLI exit/reconnect, Monitor 종료, USB 단절/재연결, MCU B2 reset 후 초기 상태.
9. 각 항목의 실제 로그/수치를 추가하고 관찰된 항목만 HW VERIFIED로 변경.
