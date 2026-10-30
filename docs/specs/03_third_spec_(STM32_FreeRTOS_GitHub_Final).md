# STM32 FreeRTOS Lab — GitHub 기술 포트폴리오 정리 실행 명세서

작성 기준: 2026-10-30 / Asia/Seoul

## 1. 이 명세의 목적과 실행 범위

대상 저장소: https://github.com/triton0305/freertos-stm32-lab  
대상 브랜치: `main`

이 문서만 읽고 기존 저장소의 README, 검증 기록, 명세 및 시연 자료를 정리한 뒤 GitHub에 반영한다. 기존 구현을 채용담당자와 현업 엔지니어가 이해하기 쉬운 기술 포트폴리오로 완성하는 작업이다.

이 문서는 이전 정리 명세를 대체한다. 다음 항목은 최근 대화에서 확정한 결정이며, 이전 문서의 상충하는 지시보다 우선한다.

1. 명세 Markdown **세 개 모두** `docs/specs/`에 보존하고 README 상단에 링크한다. 02번도 포함한다.
2. README는 개발자가 직접 구현과 검증을 설명하는 한국어 문체로 작성한다.
3. 본문은 구현, 설계 이유, 결과를 직접 설명한다. 반복적인 부정·방어 문구를 붙이지 않는다.
4. README 대표 자료는 **사용자가 올릴 GIF, YouTube 전체 영상 링크, two CLI 사진, button & monitor 사진**이다.
5. 긴 회귀 검증 CLI 사진은 구현 보고서에 배치한다. 영상 명령어 사진은 공개 문서에서 생략하고 설정 순서를 텍스트로 쓴다.
6. 기존 소스와 빌드 설정을 유지한다. 문서와 관련 증거 자료만 변경·추가·정리한다.
7. 이번 실행에서는 문서·자료 변경을 `main`에 커밋·Push하고 원격 결과까지 확인한다. 강제 Push는 사용하지 않는다.

이 명세에 따른 실제 작업이 요청되면 검토·계획 보고만 하고 멈추지 말고, 아래 절차에 따라 가능한 작업을 완료한다. 이 명세 파일 자체를 재작성하라는 요청에서는 파일 편집까지만 수행한다.

## 2. 변경 허용 범위와 보호 대상

### 변경 허용

- `README.md`
- `IMPLEMENTATION_REPORT.md`
- `docs/specs/`의 제공된 명세 세 개
- `docs/assets/`의 제공된 시연·검증 이미지와 사용자가 업로드한 GIF
- 유효한 내용을 이관한 뒤 공개 추적 대상에서 제거할 `CODEX_HANDOFF.md`
- 위 문서의 링크와 캡션

기존 두 문서를 중심으로 정리한다. 별도의 진행 보고용 Markdown, PDF, 중복 검증 문서를 추가하지 않는다. 상세 기록은 `IMPLEMENTATION_REPORT.md`에 모은다.

### 보호 대상

- `App/`, `Core/`, `tools/`, `tests/`의 구현 및 테스트 파일
- Task Priority, Stack, Queue, Mutex, ISR, Timing 정책
- CMake, Toolchain, Linker Script, Build Script, Preset
- 공식 의존성 버전과 커밋 핀
- 기존 Equipment Controller 및 Yocto 등 다른 저장소

소스 결함이 발견되면 위치, 재현 조건, 영향을 채팅 최종 보고에 남긴다. 문서에 맞춰 소스를 고치지 않는다. STM32 Flash나 실물 장비 제어, 소스 재생성은 수행하지 않는다. 문서 작업을 위한 불필요한 재빌드·테스트 반복도 수행하지 않는다.

## 3. 시작 시 확인할 사항

1. GitHub 플러그인으로 최신 `main` HEAD와 파일 목록을 확인한다. 로컬 저장소가 있으면 remote, 브랜치, 작업 트리 상태도 확인한다.
2. 이전 검토 커밋은 `fc304910863ce8ca7e118fd3b41b54d58dd872d4`다. 이 해시는 참고 기준이며, 현재 HEAD를 이 값으로 강제 초기화하지 않는다.
3. 현재 HEAD를 작업 기준으로 기록하고, 보호 대상 파일의 경로·blob SHA 또는 해시 목록을 보관한다. 최종 변경 전후 비교에 사용한다.
4. 기존 `README.md`, `IMPLEMENTATION_REPORT.md`, `CODEX_HANDOFF.md`와 아래 구현 파일을 읽고 설명을 대조한다.

| 파일 | 확인할 내용 |
|---|---|
| `App/app_config.h` | 애플리케이션 설정, 단위, 컴파일 타임 검사 |
| `App/rtos_lab.c`, `App/lab_time.h` | Task, Queue, Mutex, 명령 처리, deadline, debounce |
| `Core/board.c`, `Core/FreeRTOSConfig.h` | UART·GPIO·IRQ·클럭 및 커널 설정 |
| `tools/monitor.c`, `tools/cli.c`, `tools/protocol.c`, `tools/timing.c` | 실행 환경, 라우팅, HELP 표시, Timing 계산 |
| `host/CMakeLists.txt`, `tests/` | 현재 테스트 구성과 각 테스트의 대상 |
| 루트 CMake·Preset·빌드 스크립트 | 실제 빌드 절차, 의존성 핀 |

5. 현재 소스에 설정 중앙화, HELP 문법 수정, Rate 문법/범위 오류 분리, Windows CLI 표시 테스트가 반영돼 있는지 확인한다.
6. 사용자 자료는 첨부 파일, 현재 저장소, 연결된 파일 저장소에서 실제 파일을 찾아 확보한다. 파일명만으로 내용을 추정하거나 다른 문서를 대체하지 않는다.

GitHub 플러그인으로 충분한 작업은 플러그인을 우선 사용한다. 실행 환경이 다르면 같은 검증 원칙을 지키는 로컬 Git 작업으로 진행한다.

## 4. 입력 자료와 배치

사용자가 확보한 1~4번 자료는 **사진**이고, 5~6번은 **Markdown 문서**다. 동영상과 GIF는 별도 자료다.

| 입력 자료 | 공개 대상 경로 | 배치·역할 |
|---|---|---|
| 수정본 CLI·펌웨어 회귀 검증 사진 (`CLI.png` 등 실제 내용 확인) | `docs/assets/cli_regression.png` | 구현 보고서의 회귀 검증 절 |
| two CLI 사진 (`two_CLI.png`) | `docs/assets/two_cli.png` | README 다중 클라이언트 동작 절 |
| button & monitor 사진 | `docs/assets/button_monitor.png` | README 버튼·이벤트 처리 절 |
| YouTube 시연에 입력한 명령어 사진 | 추가하지 않음 | README에는 설정 순서를 텍스트로 설명 |
| 사용자 GIF | `docs/assets/auto_demo.gif`를 권장 | README 실장비 시연 절; 사용자가 올릴 예정 |
| 01번 명세 | `docs/specs/01_first_spec_(implementation_validation).md` | 초기 구현·검증 요구사항 원문 |
| 02번 명세 | `docs/specs/02_second_spec_(yocto_uart_gpio8_image).md` | 관련 Yocto 이미지 구현 명세 원문 |
| 이 03번 명세 | `docs/specs/03_third_spec_(STM32_FreeRTOS_GitHub_Final).md` | 포트폴리오 문서·자료 정리 기준 |

### 자료 처리 원칙

- 명세 세 개를 전부 보존한다. 02번은 관련 Yocto 문서라는 역할을 표시하고 STM32 펌웨어 구성요소처럼 설명하지 않는다.
- 01·02번의 요구사항을 구현 완료 증거로 취급하지 않는다. 03번은 문서 정리 지시서라는 역할로 소개한다.
- 01·02번은 제공된 내용과 제목을 보존한다. 명백한 Markdown 서식 오류와 링크 경로만 필요한 만큼 교정한다. 임의로 요약·재작성하지 않는다.
- 03번은 이번에 제공된 최신 실행 명세를 그대로 사용한다. 본문에서 지시한 내용을 이전 버전으로 되돌리지 않는다.
- 이미지를 직접 열어 내용과 가독성을 확인한다. 기존 사진과 새 파일명이 다르면 내용을 기준으로 매핑한다.
- 사진 속 숫자나 출력, 주기, 상태를 편집해 바꾸지 않는다. 생성 이미지로 검증 화면을 대체하지 않는다.
- 불필요한 터미널 여백은 무손실 크롭 등으로 줄일 수 있으나, 명령과 응답의 연결 및 증거 내용은 유지한다. 원본 사용자 파일은 보존하고 공개용 사본만 처리한다.
- 사진에 필요한 상태·이벤트가 읽히도록 배치하고, 같은 사진을 여러 문서에 반복 삽입하지 않는다.
- button & monitor 사진의 구체적인 출력은 실제 파일 확인 후 캡션에 반영한다.
- 아직 없는 파일의 내용을 만들거나, 존재하지 않는 경로를 활성 이미지·문서 링크로 추가하지 않는다.
- 자료가 누락되면 다른 문서 작업을 계속한다. 01·02번 또는 필수 사진이 끝내 확보되지 않으면 최종 보고에 정확한 누락 항목을 남기고 해당 자료 연결까지 완료했다고 주장하지 않는다. GIF는 아래의 예정된 사용자 작업으로 처리한다.

## 5. README의 확정 구성

아래 순서를 따른다. 제목은 자연스러운 한국어로 다듬되 순서와 역할은 유지한다.

### 5.1 프로젝트 제목·소개

제목은 `STM32 FreeRTOS Lab`으로 작성한다. 소개는 2~3문장으로 무엇을 구현했는지 설명한다.

방향 예시:

> STM32F411RE에서 FreeRTOS 기반 LED 제어와 버튼 입력을 구현했습니다. 명령 처리, 제어, 상태 생성, UART 송신을 네 개의 Task로 분리하고 Queue와 Mutex, Task Notification으로 연결했습니다. Windows Monitor·CLI로 제어 명령을 전달하고 보드의 상태와 이벤트를 관찰했습니다.

핵심 기술은 한 줄 또는 짧은 목록으로 정리한다.

`NUCLEO-F411RE · Cortex-M4F · C11 · FreeRTOS V11.3.1 · Queue / Mutex / Task Notification · UART / EXTI · Windows TCP Monitor / CLI`

### 5.2 프로젝트 명세 — 상단 노출

**프로젝트 소개 바로 아래, 핵심 성과와 GIF보다 위에** 명세 링크 표를 둔다. 긴 설명을 앞에 넣지 않는다.

| 표시 제목 | README에서 연결할 경로 | 설명 |
|---|---|---|
| 01 · 구현·검증 명세 | `docs/specs/01_first_spec_(implementation_validation).md` | 초기 구현 요구사항과 검증 기준 |
| 02 · 관련 Yocto 이미지 명세 | `docs/specs/02_second_spec_(yocto_uart_gpio8_image).md` | UART·GPIO 이미지 관련 요구사항 |
| 03 · 포트폴리오 정리 명세 | `docs/specs/03_third_spec_(STM32_FreeRTOS_GitHub_Final).md` | 문서 구성과 GitHub 정리 기준 |

실제 Markdown에서는 세 행 모두 클릭 가능한 상대경로 링크로 작성한다. 괄호가 포함된 파일명은 GitHub Markdown에서 링크가 정상 해석되도록 대상 경로를 `<...>`로 감싼다. 연결된 파일이 실제 존재하는지 검사한다.

### 5.3 핵심 구현·성과

다음 내용을 4~6개 항목 또는 간결한 표로 보여준다.

- 역할에 따라 Application Task 4개를 분리하고 우선순위 4/3/2/1을 구성했다.
- UART 명령 응답(RSP)과 주기 상태(STAT), 비동기 이벤트(EVT)를 구분했다.
- Queue 기반 제어 요청, 짧은 Mutex 구간, ISR Notification을 사용했다.
- Windows CLI 두 개의 연결과 교차 제어·상태 조회를 확인했다.
- 보드에서 LED AUTO 주기 변경과 버튼 입력을 확인하고 UART 이벤트의 Timing을 측정했다.
- Linux 3개, Windows 5개 자동 테스트를 통과했다.

시험 조건과 상세 수치는 검증 절에서 설명한다. 동일한 성과를 여러 절에서 장황하게 반복하지 않는다.

### 5.4 실장비 시연 — GIF와 전체 영상

GIF는 사용자가 직접 올린다. 이미 저장소에 올라왔다면 실제 파일을 확인해 연결한다. 아직 없으면 GIF를 생성하거나 사용자에게 다시 촬영하도록 요구하지 말고, 이 절에 보이지 않는 HTML 주석으로 예정 경로를 표시한다. 활성 깨진 이미지나 `업로드 예정` 같은 큰 안내 블록을 노출하지 않는다. GIF 미등록은 이번 문서 정리의 차단 조건이 아니다.

GIF가 있을 때의 배치:

```markdown
![STM32 LED AUTO 주기 변경](docs/assets/auto_demo.gif)

AUTO 모드에서 토글 간격을 1000 → 500 → 100ms로 변경했습니다.

▶ [전체 동작 영상 — YouTube](https://youtu.be/3qcbrseBkAc)
```

GIF가 아직 없으면 설명과 YouTube 링크는 그대로 공개하고, 이미지 구문 대신 `<!-- GIF 삽입 위치: docs/assets/auto_demo.gif / 사용자 업로드 후 연결 -->`를 둔다. 이후 사용자가 GIF를 올리면 이 위치에서 이미지 링크를 활성화한다.

- 제공된 전체 영상 URL: https://youtu.be/3qcbrseBkAc?si=XfGECsmWR-OvbXUV
- 공개 링크는 추적 파라미터를 뺀 https://youtu.be/3qcbrseBkAc 를 사용한다.
- GIF는 STM32만 촬영한 짧은 미리보기다. Monitor 화면까지 합성하는 구성을 요구하지 않는다.
- `rate`는 ON↔OFF 토글 간격이다. 1000ms 설정에서 LED의 한 ON/OFF 주기는 2000ms다.
- 영상 명령어 사진은 생략한다. 필요하면 `led auto`, `rate 1000 → 500 → 100`, `led off`를 짧은 텍스트로 설명한다.

### 5.5 시스템 구성

실제 운영 환경 세 부분을 구분한다.

| 구성요소 | 실행·조작 위치 | 역할 |
|---|---|---|
| `cli.exe` | Windows PowerShell | 사용자 명령 입력·응답 표시 |
| `monitor.exe` | WSL2 Ubuntu 터미널에서 WSL Interop으로 실행 | Windows COM 통신, TCP 요청 중계, 이벤트·Timing 출력 |
| FreeRTOS 펌웨어 | NUCLEO-F411RE | Task 실행, GPIO·버튼·UART 처리 |

통신은 CLI ↔ TCP `127.0.0.1:19006` ↔ Monitor ↔ Windows ST-LINK VCP ↔ STM32 USART2이며, UART는 115200 8N1이다. COM7은 검증 환경의 예시로 표시한다.

설명에는 “WSL2 터미널에서 Windows 네이티브 Monitor를 실행했다”는 사실을 포함한다. WSL을 별도의 통신 중계 프로그램이나 Linux 네이티브 Monitor로 그리지 않는다. 구조도는 필요한 경우 작은 Mermaid로 작성하고 실제 통신 주체를 표시한다.

### 5.6 Task 구성과 주요 설계 판단

| Task | Priority | Stack | 역할 |
|---|---:|---:|---|
| Command | 4 | 512 words | RX ring 소비, 명령 파싱, 조회, 제어 요청 |
| Control | 3 | 512 words | LED/GPIO, 상태 갱신, AUTO deadline, 버튼 처리 |
| Monitor | 2 | 512 words | 1초 주기 상태 생성 |
| UartTx | 1 | 512 words | UART 송신 전담 |
| Idle | 0 | 128 words | FreeRTOS 유휴 처리 |

Application Task 4개, Idle 포함 5개이며 Timer daemon은 비활성이다. 현재 코드와 대조 후 반영한다.

API 목록을 길게 나열하는 대신 다음 설계와 이유를 연결한다.

| 설계 | 설명할 내용 |
|---|---|
| UART 송신 전담 | Command·Control의 처리와 UART polling 송신을 분리 |
| RSP·Log Queue 분리 | 명령 응답 우선 송신, 포화 시 drop 카운터 |
| 제어 요청 Queue | Command 파싱과 Control 상태 변경을 연결 |
| `LabState` Mutex | Control writer, Command·Monitor reader; 복사·갱신만 짧게 보호 |
| ISR Notification | UART ring 처리와 버튼 판정을 Task로 넘김 |
| 절대 deadline | AUTO phase 유지, 지연된 slot 건너뛰기, missed 집계 |
| `vTaskDelayUntil` | Monitor의 고정 주기 release와 지난 release 건너뛰기 |
| UART 입력 복구 | 잘못된 입력·수신 유실 시 LF 기준 재동기화 |
| CLI별 ID 라우팅 | local ID → global UART ID → 원래 CLI ID 복원 |
| 설정 중앙화 | `app_config.h`의 Priority·Stack·Queue·버퍼·주기·debounce·timeout |

RX ring은 256byte 배열/255byte 유효 용량, control queue는 8, RSP는 8×320byte, log는 12×320byte로 구분한다. 실물 및 Host 검증 설명과 수치는 현재 코드에 근거한다.

커널 설정은 `FreeRTOSConfig.h`, 핀·주변장치 설정은 Board, Host 실행 옵션은 Host 계층에서 관리한다. SysTick 1000Hz와 HAL TIM5 timebase의 역할도 짧게 설명한다.

Priority Inheritance는 Mutex 지원 설정으로 설명한다. 우선순위가 실제로 상속되는 경합을 관찰했다는 서술은 추가하지 않는다.

### 5.7 실행·검증 결과

- two CLI 사진: 두 연결, PING 및 조회, 교차 제어·상태 확인을 설명한다.
- button & monitor 사진: B1 입력, 버튼 카운트, UART 이벤트와 주기 상태를 설명한다.
- 두 사진은 각각 해당 기능 설명 바로 아래에 배치한다.
- 빌드·Host·HW 결과, Timing, 메모리 요약을 표로 정리한다.
- 긴 회귀 검증 사진은 이 절에 중복 삽입하지 않고 구현 보고서로 연결한다.

검증 수치는 아래 7절을 사용한다. 측정 환경과 조건을 표에 붙여 결과를 이해할 수 있게 쓴다.

### 5.8 빌드·실행·명령어

실제 저장소의 CMake·스크립트·옵션을 확인해 재현 가능한 명령을 작성한다. shell별 코드 블록을 구분한다.

기본 빌드 흐름은 다음 명령을 현재 파일과 대조해 사용한다.

```bash
git clone https://github.com/triton0305/freertos-stm32-lab.git
cd freertos-stm32-lab
cmake --preset Debug
cmake --build --preset Debug
./scripts/build-windows.sh
cmake -S host -B build/host-linux -G Ninja
cmake --build build/host-linux
ctest --test-dir build/host-linux --output-on-failure
ctest --test-dir build/host-win --output-on-failure
```

Monitor 예시는 WSL 터미널에서 `./build/host-win/monitor.exe --serial COM7 --baud 115200 --port 19006`이다. CLI는 PowerShell에서 실제 EXE 경로로 이동한 뒤 `./cli.exe --host 127.0.0.1 --port 19006`을 실행하는 Windows 예시를 작성한다. Windows 디렉터리로 Host EXE를 복사하는 절차 또는 WSL 공유 경로 접근 방법을 실제 사용 가능한 형태로 설명한다. 사용자 계정명·배포판명은 플레이스홀더로 일반화하고, 필요한 경로 변수는 정의한 후 사용한다.

필수 도구, 최초 의존성 다운로드, Windows EXE의 WSL Interop 실행 조건을 간결하게 안내한다. STM32 수동 Flash 절차는 별도 단계로 남기고 BIN 경로는 저장소 루트 기준 상대경로로 쓴다. 이번 문서 작업에서는 Flash 명령을 실행하지 않는다.

명령 표에는 `help`, `ping`, `status`, `tasks`, `queue`, `led on`, `led off`, `led auto`, `rate N`, `exit`를 넣는다. `rate` 허용 범위는 100~5000ms, 기본 500ms이며 `exit`는 Host CLI 종료다.

### 5.9 소스 구조·상세 기록

디렉터리를 옮기지 않고 `App`, `Core`, `tools`, `tests`, `host`, `cmake`, `scripts`, `docs`의 역할을 짧은 표로 안내한다. 상세 이력은 `IMPLEMENTATION_REPORT.md`로 연결한다.

### 5.10 기술적 한계

본문 곳곳에 한계 문장을 반복하지 말고 마지막에 짧게 모은다. 아래 항목 중 실제 설계 제약을 우선한다.

- Queue 포화 시 drop과 명령 timeout이 발생하며, timeout 후 제어 상태는 조회로 확인한다.
- Timing 수신값에는 MCU 클럭과 UART·Host 수신 지연이 함께 반영된다.
- fault hook은 정지 정책이며 자동 복구가 필요한 적용에는 watchdog 등 추가 설계를 고려한다.

추가 검증 과제는 상세 보고서에 모은다. 산업용 기능 안전성 같은 프로젝트 목적과 거리가 있는 경고를 README에 반복해서 붙이지 않는다.

## 6. 문체와 기술 서술 규칙

공개 문서는 프로젝트 개발자가 작성한 시점으로 서술한다. 제3자의 평가나 작업 대화처럼 쓰지 않는다.

| 피할 표현 | 사용할 방향 |
|---|---|
| “사용자가 Task 4개를 구현했다” | “처리 역할에 따라 Task를 4개로 분리했습니다.” |
| “Codex가 테스트 통과를 보고했다” | “Linux 3개, Windows 5개 테스트를 통과했습니다.” |
| “Work가 버튼을 검증한 것은 아닙니다” | “B1 입력 후 버튼 카운트와 UART 이벤트 출력을 확인했습니다.” |
| “오실로스코프 측정은 아닙니다” | “Windows QPC와 UART 이벤트 수신 시각으로 간격을 측정했습니다.” |
| “동시 연결이 고부하 경쟁 검증까지 된다는 것은 아닙니다” | “CLI 두 개를 연결해 교차 명령과 상태 조회를 확인했습니다.” |

- 문단은 구현 → 설계 이유 → 검증 결과로 연결한다.
- “검증 범위가 아닙니다”, “구현되지 않았습니다”, “보장하는 것은 아닙니다” 등을 습관적으로 덧붙이지 않는다.
- 방법·조건·수치를 구체적으로 쓰고 성과를 명료하게 설명한다.
- 개발자 문체를 사용해도 시험 수행자, 측정 방법, 관찰 범위를 바꾸거나 이 작업에서 새로 재검증한 것으로 꾸미지 않는다.
- 공개 본문에 사용자·Codex·Work의 역할 분담 대화를 반복하지 않는다. 상세 보고서의 시험 환경·기록 출처 표에서 날짜와 자료 종류를 간단하게 추적한다.
- AI 활용 사실을 묻거나 별도 설명이 필요하면 실제 작업 방식을 따른다. 단독 수작업 개발 등의 근거 없는 기여 주장은 추가하지 않는다.
- 장식성 배지·사진·구조도를 늘리지 않는다. 표와 작은 다이어그램은 정보 전달에 필요한 곳에 사용한다.

## 7. 반영할 검증 사실과 수치

아래는 사용자가 전달한 구현 담당 결과 및 실장비 실습 기록이다. 해당 자료를 근거로 문서화한다. 실제 첨부·소스와 충돌하는 부분이 있으면 근거가 있는 표현으로 좁히고 최종 보고에 차이를 남긴다.

### 7.1 빌드·Host 회귀 검증 — 2026-10-30

| 항목 | 결과 |
|---|---|
| ARM Debug Build | PASS |
| Windows Monitor·CLI Build | PASS |
| Linux CTest | 3/3 PASS |
| Windows CTest | 5/5 PASS |
| Rate 파서 | 31개 사례 통과 |
| Firmware Dispatch·HELP | 386 checks 통과 |
| Host Protocol | 40 checks 통과 |
| 다중 CLI | 34 checks 통과 |
| CLI 표시 | 6 checks 통과 |

Host firmware 시험은 실제 Dispatch에 Queue·Mutex test double을 연결한다. 시험 설명에는 이 구성을 긍정적으로 기술하고 MCU Task 스케줄러를 실행한 시험으로 확장하지 않는다.

이 대화의 별도 검토 환경에서는 GCC 직접 빌드로 portable protocol 40 checks, firmware dispatch 386 checks/Rate 31개, 10초 timing·deadline·debounce 시험을 재실행해 통과했다. CMake가 없어 CTest를 실행한 기록과는 구분한다. 이 추가 기록은 필요할 때 상세 보고서에만 사용하며 Windows·ARM을 새로 실행한 것으로 쓰지 않는다.

### 7.2 메모리

| 지표 | 초기 빌드 | 수정본 빌드 | 변화 |
|---|---:|---:|---:|
| Flash | 31,980 bytes | 32,132 bytes | +152 bytes |
| RAM | 36,680 bytes | 36,680 bytes | 동일 |

수정본의 보드 실행 중 Free Heap은 16,680 bytes로 관찰됐다. 링커 RAM 사용량과 RTOS heap 잔여량은 표의 서로 다른 지표로 표시한다. 초기 BIN SHA-256은 초기 빌드 기록에만 둔다. 수정본 해시가 확보되지 않으면 새 해시를 만들거나 초기 해시를 재사용하지 않는다.

### 7.3 실장비 확인 — 2026-10-30

- STM32CubeProgrammer로 수정 펌웨어 기록, Download Verify 및 MCU Software Reset 성공.
- FreeRTOS Task 실행과 UART 통신 확인.
- Application Task 4개/전체 5개, Priority 4/3/2/1/0 확인.
- HELP 명령별 줄바꿈 및 실제 명령 문법 확인.
- 오류 Rate 입력 뒤 정상 명령 처리와 상태 조회 확인.
- 정상 시험 중 UART RX/TX 오류와 Queue drop 0 관찰.
- 버튼 입력 후 카운트·이벤트와 Notification 수 변화 확인.
- PowerShell CLI 두 개 연결, 각각 PING 및 STATUS/TASKS/QUEUE 조회, 교차 LED 제어·상태 조회 확인.

### 7.4 회귀 검증 사진에 대응하는 명령 순서

공개 제목은 **수정본 CLI·펌웨어 회귀 검증**으로 쓴다. 사용자가 붙인 Test 09라는 이름은 기존 README의 Restart Test 09와 혼동되므로 본문 제목에서 사용하지 않는다.

```text
help
ping
status
tasks
queue
rate 500abc
rate 500.1
rate 100.1
rate abc
rate 99
rate 5001
rate 100
led auto
status
rate 500
status
led off
status
led on
status
led auto
status
queue
```

문법 오류 네 개는 `ERR bad_rate`, 범위 오류 두 개는 `ERR rate_range_100_5000`이다. 오류 입력 여섯 개에 따른 `protoerr` 증가와 이후 정상 명령에 의한 상태 확인을 사진의 실제 순서에 맞춰 설명한다. 사진에 없는 명령이나 결과를 추가 실행한 것처럼 서술하지 않는다.

### 7.5 Timing

| 항목 | 설정한 토글·생성 간격 | Host 수신 평균 간격 | 간격 수 | Host 경과 | MCU timestamp 증가 |
|---|---:|---:|---:|---:|---:|
| LED 이벤트 | 500ms | 494.200ms | 30 | 14,826ms | 15,000ms |
| STATUS | 1000ms | 989.786ms | 14 | 13,857ms | 14,000ms |

측정 방법은 Windows QueryPerformanceCounter와 UART 이벤트·상태 수신 시각 비교다. MCU 값은 프레임에 기록된 timestamp를 사용했다. 두 시간 기준 사이에 약 1% 수준의 차이를 관찰했으며 원인 분석은 별도 과제로 둔다. GPIO 출력 파형의 정밀 지터 값이나 보장 성능으로 수치를 바꾸지 않는다. GIF·영상은 동작 시연 자료로 설명한다.

### 7.6 버튼·Notification

- 기록에서 `buttons` 0 → 1 → 2, `btnnotify` 0 → 2 → 6을 관찰했다.
- two CLI 사진처럼 더 나중 상태가 보이면 그 사진의 실제 숫자를 그대로 설명하고, 서로 다른 시점의 출력을 하나의 연속 기록처럼 합치지 않는다.
- Notification 수는 ISR 알림 수이며 확정된 버튼 입력 수와 구분한다.
- 소스의 50ms debounce 설정과 실물의 카운트·이벤트 관찰을 각각 설명한다.

### 7.7 상세 보고서에 남길 추가 검증 과제

- Mutex Priority Inheritance 강제 경합 측정
- 실제 Queue Full 및 부하 제거 후 복구
- UART ORE 주입 및 물리 통신 장애 복구
- 장시간 고부하 실행
- GPIO 파형 계측
- watchdog·fault 자동 복구 설계

성과 절에는 실제 수행한 시험만 쓰고, 추가 과제는 상세 보고서 끝에 간결하게 모은다.

## 8. 기존 문서·링크 정리

### README.md

5절 순서로 개편한다. 2026-10-29의 “새 펌웨어 Flash 전”, “HW VERIFIED 없음”, Windows 4/4를 현재 상태처럼 보여주는 문구는 최신 수정본 결과로 갱신한다. 불필요한 작업자 지시, 패키지 설치 실패 과정, 수정 파일 전체 목록은 README에서 덜어낸다.

### IMPLEMENTATION_REPORT.md

- 첫 부분에 현재 수정본 결과 요약과 검증 자료 링크를 둔다.
- 이후 초기 구현·검증(2026-10-29), 수정·회귀 검증(2026-10-30), 수정본 실장비 확인을 구분한다.
- 초기 수치·해시·Windows 4/4·당시 HW 상태는 초기 기록이라는 제목 아래 보존한다. 최신 수치로 덮어쓰지 않는다.
- 최신 결론은 수정본 Host/HW 결과로 작성하고, 초기 “HW VERIFIED 없음”이 현재 결론처럼 남지 않게 한다.
- 회귀 검증 사진은 이 문서에 삽입한다. README와 사진 경로의 상대 위치 차이를 반영한다.
- 시험 날짜, 대상 버전, 도구·환경, 자료 종류를 간단한 표로 추적한다. Git 커밋과 당시 binary의 동일성이 확인되지 않으면 확정적인 대응을 만들어 쓰지 않는다.

### CODEX_HANDOFF.md

유효한 빌드 방법, test double 설명, 검증 범위를 다른 문서로 이관한 뒤 공개 추적 대상에서 제거한다. 사용자의 로컬 인계 원본은 유지한다. 로컬 Git에서는 필요하면 `git rm --cached`와 로컬 exclude를 사용하고, 다른 파일을 포괄적으로 지우거나 Git 이력을 재작성하지 않는다. GitHub API를 사용한다면 현재 브랜치의 문서 삭제만 반영한다.

### 링크·증거 관리

- `build/*.log`는 공개 저장소에 없을 수 있다. 실제 파일 존재를 확인한다.
- 제공된 로그가 있으면 필요한 것만 검토해 `docs/assets/` 등 문서 증거 경로에 선별 보존하고 링크를 고친다.
- 없는 로그 링크는 제거하고 필요한 경우 “당시 로컬 빌드 로그”로 간단하게 출처를 설명한다. 새 로그를 합성하지 않는다.
- 개인 계정명·PC 절대경로는 공개 실행 안내에서 일반화한다. 인증정보는 공개 자료에 포함하지 않는다.
- 상단 명세의 원문 보존과 공개 정보 점검이 충돌하면 실제 민감 값만 최소한으로 가리고 변경 사항을 보고한다. 과거 요구사항과 현재 결과의 차이는 제목·설명으로 구분한다.

## 9. 실행 순서와 GitHub 반영

1. 최신 `main`과 작업 트리를 확인하고 기준 HEAD·보호 대상 해시를 확보한다.
2. 소스와 기존 문서를 읽어 구현·검증 설명을 대조한다.
3. 명세 세 개, 사진, 기존 GIF를 확인해 입력 자료를 매핑한다.
4. `docs/specs/`에 명세 세 개를 배치한다. 제공 자료만 사용한다.
5. 공개에 필요한 사진 사본을 `docs/assets/`에 배치한다. 영상 명령어 사진은 추가하지 않는다.
6. README를 확정 순서·개발자 문체로 개편한다. GIF가 없으면 숨은 삽입 위치와 YouTube 링크를 남긴다.
7. 구현 보고서를 최신 요약과 시점별 이력으로 정리한다.
8. HANDOFF의 유효 내용 이관을 확인하고 공개 추적 대상에서 제거한다.
9. Markdown 표·코드 블록·상대경로·명세 링크·이미지 링크를 검사한다. 가능한 경우 렌더링도 확인한다.
10. 변경 파일 목록과 diff를 검토한다. 보호 대상 파일의 경로·내용·모드 변경이 없는지 기준 HEAD와 비교한다.
11. 원격 HEAD를 다시 확인한다. 다른 작업자 변경이 있으면 문서 변경을 최신 내용과 조정하고 다시 검사한다. 소스 차이가 예상 밖이거나 충돌을 안전하게 해소할 수 없으면 반영을 멈추고 정확한 사유를 보고한다.
12. 문서·명세·사진 변경만 명시적으로 stage/commit한다. `git add .` 같은 일괄 추가는 사용하지 않는다.
13. `main`에 fast-forward 가능한 방식으로 Push한다. GitHub API라면 현재 HEAD를 부모로 하는 문서 전용 커밋을 만들고 기준 SHA를 확인하며 ref를 갱신한다.
14. 원격에서 최신 HEAD와 변경 파일을 다시 조회한다. README·명세·보고서·사진 경로 및 HANDOFF 제거를 확인한다.
15. 기준 HEAD와 최종 HEAD의 변경 목록을 대조해 소스·테스트·빌드 설정이 유지됐는지 확인한다.

명세에서 허용한 문서·자료 반영을 위해 별도 승인 질문을 반복하지 않는다. 쓰기 권한이나 필수 입력이 실제로 부족하면 가능한 정리를 마치고 미완료 항목을 구체적으로 보고한다.

## 10. 완료 기준

- README가 최신 소스 구조와 실제 동작을 설명한다.
- 소개 바로 아래에 명세 세 개의 정상 링크가 있다.
- 명세 세 개가 지정된 `docs/specs/` 경로에 있고 역할이 구분된다.
- 개발자 문체로 구현·설계 이유·검증 결과를 설명한다.
- 부정·방어 문장이 본문에 반복되지 않는다.
- YouTube 링크가 시연 설명 아래 있으며, GIF는 실제 파일이 있을 때만 연결한다.
- two CLI와 button & monitor 사진이 해당 설명에 연결된다.
- 긴 CLI 회귀 검증 사진은 상세 보고서에 배치되고 영상 명령어 사진은 생략된다.
- 최신 Host·HW·메모리·Timing 결과가 정확한 방법·조건과 함께 반영된다.
- 초기 결과와 수정본 결과가 구분된다.
- HANDOFF의 유효 내용이 이관되고 공개 추적 대상에서 제거된다.
- 활성 깨진 링크, 개인 실행 경로, 민감정보가 정리된다.
- 펌웨어·Host·테스트·빌드 설정의 변경이 없다.
- 문서·자료 변경이 `main`에 반영되고 원격 재조회로 확인된다.

사용자 업로드 예정인 GIF만 빠진 상태는 문서 정리 완료와 구분해 “GIF 사용자 업로드 후 연결”로 최종 보고한다. 그 외 필수 입력 누락이나 원격 미반영은 부분 완료로 명확히 보고한다.

## 11. 최종 보고

실행 결과는 채팅으로 보고하고 별도의 보고 파일은 만들지 않는다.

1. 변경한 문서·자료와 핵심 수정 내용
2. README 최종 순서와 상단 명세 세 개의 경로
3. 사진·GIF·YouTube 배치 결과 및 사용자에게 남은 GIF 작업
4. 초기/수정본 검증 기록의 주요 정리 내용
5. HANDOFF 정리 결과
6. 보호 대상 소스·테스트·빌드 설정 무변경 확인
7. 실제 Commit ID와 GitHub 링크
8. 원격 재조회 결과
9. 발견한 코드 결함, 누락 자료 또는 남은 작업

최종 목표는 제공된 구현·검증 근거를 일관된 문서와 시연 자료로 연결해, 개발자가 직접 설명하는 읽기 쉬운 STM32 FreeRTOS 기술 포트폴리오를 완성하는 것이다.
