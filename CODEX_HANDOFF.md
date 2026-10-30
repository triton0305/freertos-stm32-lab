# 다음 작업자 인계 — 2026-10-29

이 프로젝트는 독립 C11 STM32F411RE FreeRTOS Lab이다. `README.md`의 절차와 `IMPLEMENTATION_REPORT.md`의 실제 로그/검증 범위를 먼저 읽는다. 과거 초안의 PASS를 재사용하지 않는다.

현재 ARM ELF/BIN/MAP 및 Linux 3/3, Windows 4/4 Host tests 성공. WSL MinGW build 및 실제 Windows EXE 실행, COM7에서 기존 펌웨어 출력 수신까지 확인했다. **Lab 펌웨어는 Flash하지 않았고 HW VERIFIED는 없다.**

```bash
cmake --preset Debug
cmake --build --preset Debug
./scripts/build-windows.sh
cmake -S host -B build/host-linux -G Ninja
cmake --build build/host-linux
ctest --test-dir build/host-linux --output-on-failure
ctest --test-dir build/host-win --output-on-failure
```

다음 단계는 사용자의 수동 Flash 후 README Test01~09 실물 관찰이다. Hardware timing은 Windows QPC와 MCU 이벤트 timestamp를 10초 이상 비교한다. Host 테스트는 합성 MCU 입력이며 보드 시간 정확성 증거가 아니다.

금지: 자동 Flash, Git commit/push, Equipment Controller 수정/로컬 소스 의존, Python Host 변경, 실측 없는 HW VERIFIED. 보드 Flash 시 기존 펌웨어가 교체됨을 명시해야 한다. Flash 명령은 README/보고서에만 제시되어 있다.

소스 수정 후 적합한 테스트만 재실행하고 최종 binary hash/메모리/결과를 갱신한다. 테스트 fake 헤더는 host firmware_tests에만 포함되며 ARM은 공식 FreeRTOS를 사용한다.
