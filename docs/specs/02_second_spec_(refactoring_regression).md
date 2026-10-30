현재 작업 디렉터리 \~/projects/stm32-freertos-lab 의 최신 소스를 기준으로 버그 수정, 유지보수성 개선, 회귀 검증을 수행하라.

[최우선 원칙]

- 현재 실장비 검증을 통과한 기능의 동작과 Timing 정책을 유지한다.
- 다른 프로젝트, 특히 기존 Equipment Controller는 절대 수정하지 않는다.
- Git commit/push 금지, 자동 Flash 금지.
- 보고용 .md, PDF 등 별도 파일을 생성하지 않는다. 작업 결과는 터미널과 최종 채팅으로만 보고한다.
- 기존 파일 구조를 먼저 분석하고, 실제로 필요한 범위만 리팩터링한다. 보여주기 위한 과도한 파일 분리는 금지한다.

[1. HELP 출력 결함 수정]\
현재 펌웨어 HELP는 led_on, led_off, led_auto 같은 잘못된 명령어를 안내한다. 실제 입력 명령은 led on, led off, led auto이다.

- 사용자에게 보이는 HELP에는 실제로 동작하는 명령어만 정확하게 표시한다.
- Windows CLI에서 help 실행 시 명령어 하나당 한 줄씩 보기 좋게 출력한다.
- help, ping, status, tasks, queue, led on, led off, led auto, rate N (100\~5000 ms)을 표시한다.
- exit가 Host CLI 전용 명령이라면 Host에서만 표시한다.
- 기존 UART 단일 라인 프레이밍, Monitor 응답 ID 라우팅, 비동기 STAT/EVT 처리와 다중 CLI 동작을 유지한다. 필요하면 Host 표시 계층에서 줄바꿈한다.

[2. Rate 파서 버그 수정]

- rate abc, rate a, rate z, rate 100.1, rate 500.1, rate 500abc => ERR bad_rate
- rate 99, rate 5001 => ERR rate_range_100_5000
- rate 100, rate 500, rate 5000 => 정상 처리
- 문자열 전체를 유효한 십진 정수로 파싱한 다음 범위를 검사한다.
- 숫자 뒤의 문자, 소수점, 정수 오버플로를 잘못 허용하지 않는다.
- 잘못된 명령에서는 기존 LED mode/rate가 유지되어야 한다.
- 공백, 빈 인자, 부호, 앞뒤 문자, 경계값도 회귀 테스트에 포함한다.

[3. 설정값 중앙화]

- 애플리케이션에서 반복 사용하는 하드코딩된 설정값을 찾아 역할별 설정 헤더로 정리한다.
- Task Priority와 Stack Size, Queue Depth, UART RX/TX 버퍼 크기, CLI 명령 길이 제한, LED 기본 Rate 및 허용 범위, 버튼 디바운스 시간, 주기 실행 간격 등은 app_config.h 같은 명확한 애플리케이션 설정 파일에서 관리하도록 한다.
- FreeRTOSConfig.h의 커널 설정, Tick, 인터럽트 관련 설정은 기존 FreeRTOSConfig.h에 유지한다.
- GPIO 핀과 MCU 주변장치 설정은 기존 Board 계층에서 관리한다.
- Windows Host의 COM/TCP 설정은 Host 계층에 남기고 실행 옵션을 유지한다.
- 런타임에 변하는 LED 상태, 카운터, Task 상태 등은 설정 상수로 이동하지 않는다.
- 상수 이름에 단위를 명확히 표시한다. 예: \_MS, \_BYTES, \_WORDS.
- 필요한 경우 컴파일 타임 검증으로 상수 간 관계를 확인한다.

[4. 구조 및 가독성 정리]

- Task 함수, Queue 처리, Mutex 보호 구간, UART RX/TX, 명령 파서, 상태 관리가 명확히 구분되는지 점검한다.
- 중복 코드, Magic Number, 중복 선언, 불필요한 전역 변수, 사용되지 않는 코드와 헤더를 정리한다.
- 함수 및 변수 명명 방식을 일관되게 유지한다.
- 공용 API와 내부 static 함수의 경계를 정리하고, 필요하지 않은 전역 심볼 노출을 줄인다.
- Mutex 획득/반환 경로, 오류 경로, ISR과 Task 간 데이터 접근을 검토한다.
- ISR에서 호출하는 FreeRTOS API, UART 전송 경로, Lock 범위를 변경할 경우 실질적인 안전성 근거를 확보한다.
- 불필요한 리팩터링으로 기능 구조를 전면 변경하지 않는다.
- 자동 생성 코드와 외부 라이브러리 코드는 가능한 한 수정하지 않는다.
- 꼭 필요한 설명만 주석으로 남기고, 단순 코드 동작을 반복하는 주석은 남발하지 않는다.

[5. 기존 기능 유지]

- Command(4), Control(3), Monitor(2), UartTx(1), Idle(0) 우선순위를 유지한다.
- 기존 Queue Depth, Task Stack, 1ms Tick, TIM5 HAL Timebase, ISR 우선순위 정책을 유지한다.
- LED AUTO 절대시간 Deadline과 100\~5000ms 토글 간격을 유지한다.
- 1초 STATUS 주기, B1 50ms 디바운스, 상태 Mutex 보호, UART 오류 복구 정책을 유지한다.
- 기존 TCP 다중 CLI, 응답 라우팅 및 RSP 우선 송신 정책을 유지한다.
- 위 변경이 꼭 필요하다면 사유와 영향 범위를 명확히 보고한다.

[6. 검증]

- ARM Debug 펌웨어 빌드
- Windows Monitor/CLI 빌드
- Linux/Windows Host 자동 테스트
- HELP 출력 회귀 테스트
- Rate 정상/비정상/경계값 테스트
- 명령 오류 후 정상 ping/status 복구 테스트
- 기존 프로토콜·다중 CLI·UART·Queue 관련 회귀 테스트
- 적용 전후 FLASH/RAM 사용량 및 git diff 확인
- 실제 실행한 테스트의 PASS/FAIL 건수를 정확히 기록한다.
- 실장비를 직접 측정하지 않은 항목을 HW VERIFIED라고 표시하지 않는다.

[7. 최종 채팅 보고]

1. 발견한 결함과 정확한 원인
2. 실제 변경한 파일 및 주요 변경 내용
3. 설정값 정리 전후 구조
4. 빌드·Host 테스트 실제 결과
5. 기존 기능에 대한 회귀 위험과 미검증 항목
6. 사용자가 직접 Flash하고 확인해야 할 최소 HW 검증 절차

주의: 최신 사용자 코드를 기준으로 작업하며, 작업 전 git status를 기록하고 기존 미커밋 변경사항을 보존한다.
