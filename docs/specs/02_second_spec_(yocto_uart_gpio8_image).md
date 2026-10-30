# Yocto — Raspberry Pi 4B UART + GPIO 8채널 토글 통합 이미지 구현 명세서

> **문서 버전:** 1.0 / 2026-10-29  
> **용도:** Codex에 **이 파일 하나만 제공**하여 구현·Host 빌드·정적 검증까지 수행시키는 작업 명세  
> **목표 직무:** 차량용 Core SW·플랫폼 SW  
> **결과 저장소:** [`triton0305/yocto-rpi4-lab`](https://github.com/triton0305/yocto-rpi4-lab)  
> **재사용 원본:** [`triton0305/linux-system-programming`](https://github.com/triton0305/linux-system-programming) / `ch07-platform-gpio/`  
> **명령 권한:** Codex는 코드 작성과 **실제 Host 빌드·검증까지 진행**한다. **Git commit/push 및 SD·USB 저장장치 기록은 금지**한다.

---

## 0. Codex에게 전달하는 최상위 지시

**이 문서를 읽고 설계안만 작성하는 데서 멈추지 말고, 접근 가능한 기존 WSL2 작업 환경에서 구현과 BitBake 빌드, 산출물 검증까지 실행하라.** 기존 환경을 먼저 읽기 전용으로 조사한 후 필요한 파일만 최소 변경한다.

- 작업 범위는 **Yocto 이미지 통합**이다. 기존 GPIO 드라이버, C 앱, IRQ 또는 토글 알고리즘을 새로 구현하지 않는다.
- 기존 사용자 작업물, `meta-raspberrypi` 원본, 기존 Linux 커널 소스 및 다른 프로젝트를 임의 수정하지 않는다.
- `git commit`, `git push`, 태그 생성, 원격 브랜치 갱신, 기존 물리 SD/USB 기록 작업을 수행하지 않는다. 사용자가 직접 검토·커밋·배포한다.
- 기존 활성 BitBake Layer와 빌드 설정은 **실제 파일을 확인**하고, 필요한 변경 시 기존 내용을 백업·비교한 후 최소 범위로 수정한다. 기존 운영 중인 SD의 `config.txt`/`cmdline.txt`는 건드리지 않는다. **새로 생성하는 WIC 이미지 내부 부팅 설정**은 명세에 맞게 생성한다.
- 실장비에서 직접 실행하지 않은 동작을 `HW VERIFIED`로 표기하지 않는다. Host 빌드가 되지 않거나 외부 조건이 불명확하면 정확한 실패 원인과 미검증 상태를 보고한다.
- 변경 파일과 실측된 빌드 결과만 보고하고, 추정한 파일명·패키지명·시간·성공 여부를 확정 사실처럼 쓰지 않는다.

**정상 완료 기준:** 하나의 Raspberry Pi 4B용 WIC 이미지에 ARM64 Linux, UART 콘솔/로그인, 활성 8채널 Device Tree Overlay, `lab_gpio.ko`, 원본 `lab_gpio_test` 프로그램, 부팅 자동 시작이 통합되며, Host에서 이미지 내부 구성과 무결성이 검증된 상태. 이후 사용자가 Pi 실장비에서 별도 수락 검증한다.

---

## 1. 목표와 제외 범위

### 1.1 사용자 경험

```text
WSL2 / x86-64 Host
    └─ Yocto 6.0.4 / poky / raspberrypi4-64
         └─ bitbake yocto-gpio-toggle-image
              └─ WIC 전체 SD 이미지
                   ├─ Raspberry Pi boot files + ARM64 Kernel + DTB
                   ├─ FAT Boot partition / overlays/lab-gpio-eight-overlay.dtbo
                   ├─ RootFS + UART console/login 설정
                   ├─ lab_gpio.ko (8개 DT platform device와 바인딩)
                   ├─ lab_gpio_test (원본 C 사용자 앱)
                   └─ boot init script (한 번만 자동 시작)

사용자: 이미지 SD 기록 → Pi 4B 전원 ON → UART 로그인 가능
        → 별도 수동 명령 없이 8채널 버튼/LED 독립 토글
        → 재부팅 뒤 같은 동작
```

### 1.2 이번에 구현할 내용

1. 기존 검증 버전과 일치하는 소스 고정 및 Custom Layer 통합.
2. Kernel Module Recipe, User-space App Recipe, 8채널 DT Overlay Recipe.
3. 타깃 init 방식에 맞춘 자동 모듈 적재·앱 시작·중지·상태 확인.
4. `core-image-minimal` 기반 Custom Image Recipe 및 부팅 파티션 통합.
5. Recipe별 실제 빌드, WIC 이미지 빌드, Host 이미지 내부 정적 검증.
6. 재현 가능한 빌드 문서, SHA-256, Package Manifest, 사용자 HW 수락 테스트 안내.

### 1.3 금지할 범위 확장

- 신규 GPIO 드라이버, ISR/IRQ·GPIO interrupt, 커널 이벤트 큐·workqueue·timer 도입.
- 원본 `lab_gpio_test.c`를 다른 토글 프로그램으로 교체하거나 기존 debounce/타이밍 변경.
- `thermal-core`, `hello-drv`, stress-test 일괄 설치, SSH 서버, 웹 UI, GUI, 불필요한 네트워크 서비스 추가.
- `meta-raspberrypi` 원본 변경, 전체 커널 또는 BSP 새로 작성, 다른 Yocto 빌드 환경 새로 구성.
- 증거 없는 `HW VERIFIED`, 임의의 핀 변경, 기존 SD 부팅 설정 무단 덮어쓰기.

기본 `core-image-minimal`과 BSP가 부팅에 요구하는 필수 구성은 유지한다. 이미 기본 이미지에 존재하는 패키지를 인위적으로 제거하라는 뜻이 아니다.

---

## 2. 기존 환경과 소스 기준선

| 항목 | 기준 또는 반드시 확인할 값 |
| --- | --- |
| Host | 기존 **센터 WSL2 Ubuntu 22.04** 빌드 환경 우선 재사용 |
| Target | Raspberry Pi 4 Model B / AArch64 |
| Yocto | 6.0.4 `wrynose` / `DISTRO=poky` |
| MACHINE | `raspberrypi4-64` |
| 초기 이미지 | `core-image-minimal` |
| Custom Image | `yocto-gpio-toggle-image` |
| 이전 타깃 커널 | 실검증 기록 `6.18.33-v8`; 현재 실제 빌드/타깃 버전 반드시 재확인 |
| UART | 기존 실검증 `ttyS0` / 115200 baud / 8N1; 실제 부팅 설정의 `serial0` 대응 확인 |
| BSP | 기존 `meta-raspberrypi` |
| 사용자 Layer | 기존 **활성** `meta-yocto-lab` 재사용, 중복 활성 등록 금지 |
| 원본 GPIO SHA | `56a431d1e633d90f1bfe98a3a7bfaa026494862c` (출발점) |
| 소스 SHA | **실제 8채널 HW 검증에 사용된 커밋**이 다르면 그 버전을 우선 고정 |
| 언어 | Kernel Driver 및 User App 모두 기존 C 유지 |

기존 Yocto 문서에는 `core-image-minimal` 4,025 Tasks 빌드, UART root 로그인, Linux `6.18.33-v8`, `hello_drv.ko` 수동 적재 성공 기록이 있다. **이 기록은 새 GPIO 통합 이미지의 성공을 증명하지 않는다.**

기존 WSL Yocto 기본 경로 후보:

```text
~/study/rootfs/yocto/
└── bitbake-builds/poky-wrynose/
    ├── build/
    │   └── conf/{local.conf,bblayers.conf,toolcfg.conf}
    └── layers/
        ├── openembedded-core/
        ├── meta-yocto/
        ├── meta-raspberrypi/
        └── meta-yocto-lab/     # 존재 여부/실제 경로 확인
```

위는 **후보 경로**이며 실제 WSL 파일과 다르면 실제 환경을 사용한다. 결과 저장소 `yocto-rpi4-lab`의 로컬 경로도 `pwd`, `git remote -v`로 찾는다. 별도의 신규 Yocto 트리나 신규 BSP를 만들지 않는다.

### 2.1 참조할 GPIO 원본

```text
linux-system-programming/ch07-platform-gpio/
├── driver/lab_gpio.c
├── driver/Makefile
├── include/lab_gpio_uapi.h
├── userspace/lab_gpio_test.c
├── dt/lab-gpio-eight-overlay.dts
├── yocto/lab-gpio_1.0.bb
└── yocto/lab-gpio-test_1.0.bb
```

해당 GitHub SHA의 공개 `CH07_VERIFICATION.md`는 **Host ARM64 모듈·앱 빌드와 오프라인 DT 정적 검사**는 기록하지만, 그 문서의 GPIO Target 항목은 `HW VERIFIED`가 아니다. 사용자가 이후 8채널을 실제 검증했다면 **그 코드·배선·극성·검증 로그·SHA를 기준**으로 삼아 불일치를 해소한다. 물리 검증 근거를 확인할 수 없다면 현재 공개 DTS 핀 매핑을 잠정 기준으로 이미지를 Host 빌드할 수 있으나, **배선 확인 전 실장비 배포 적합 판정을 해서는 안 된다.**

### 2.2 8채널 GPIO 핀 매핑 — 공개 원본의 잠정 기준

아래 숫자는 **BCM GPIO controller line offset**이며 Raspberry Pi 40핀 헤더의 물리 핀 번호가 아니다.

| 채널 | LED GPIO offset | 버튼 GPIO offset | Overlay 논리 극성 |
| --- | ---: | ---: | --- |
| D0 | 5 | 4 | LED active-high / 버튼 active-low |
| D1 | 6 | 17 | 동일 |
| D2 | 12 | 18 | 동일 |
| D3 | 13 | 27 | 동일 |
| D4 | 19 | 22 | 동일 |
| D5 | 16 | 23 | 동일 |
| D6 | 26 | 24 | 동일 |
| D7 | 20 | 25 | 동일 |

- **실검증된 물리 회로와 다른 경우** 위 공개 DTS를 근거로 배선을 강제하지 않는다.
- GPIO14/15(UART TX/RX), 그 외 기존 사용자 GPIO consumer와 pinmux 충돌이 없는지 확인한다.
- 3.3V GPIO 규격, LED 전류 제한 저항, 버튼 확정 bias/pull 상태를 유지한다. DTS 극성은 실제 전기 회로를 자동 증명하지 않는다.
- 기존 공개 8채널 DTS는 모든 노드가 `status = "disabled"`인 오프라인 템플릿이다. 실제 **이미지용 활성 DTS**는 별도 추적 파일로 관리하고, 8개 노드를 `okay`로 구성한다.
- **8채널 Overlay 1개만** 배포한다. 기존 단일 D0 overlay를 동시에 적용하지 않는다.

---

## 3. 기능 요구사항과 수락 기준

| ID | 필수 요구 | PASS 판정 기준 |
| --- | --- | --- |
| FR-01 | Pi 4B 이미지 단독 부팅 | SD에 WIC 기록 후 수동 파일 복사 없이 Linux 부팅 및 UART 로그인 |
| FR-02 | UART 콘솔 유지 | 115200 8N1 부팅 로그·로그인·쉘 확인 |
| FR-03 | 활성 8채널 Device Tree | Live DT에 D0~D7 8개 활성 노드, 각 platform device ↔ driver bind 확인 |
| FR-04 | 드라이버 설치·로드 | 커널 ABI 일치 `lab_gpio.ko`를 이미지에 포함, 부팅 시 정상 로드, 8개 장치 노드 생성 |
| FR-05 | 초기 LED 안전 상태 | 드라이버 초기화 및 앱 자동 시작 후 모든 LED **logical OFF**; 광학/전압 실측은 HW 별도 확인 |
| FR-06 | 독립 토글 | Dn 버튼 유효 누름 1회에 LED Dn만 1회 상태 전환, D0~D7 모두 동작 |
| FR-07 | 자동 시작 | 부팅 시 `lab_gpio_test all toggle start`와 동등한 원본 로직 **단 한 번** 실행 |
| FR-08 | UART 운영 명령 | `lab_gpio_test all toggle status`, `stop`, `start` 확인 |
| FR-09 | 재부팅 재현성 | 재부팅 후 UART 로그인 및 자동 8채널 토글 유지 |
| FR-10 | 통합 산출물 | WIC 파일(필요 시 압축), SHA-256, manifest, source SHA, 빌드 기록 제공 |

**원본 동작 유지 규정:**

- `lab_gpio_test.c`는 **20ms 간격의 사용자 공간 polling**으로 버튼을 샘플링하고, **같은 상태 3회 연속 샘플**로 디바운스한다.
- 최초 `start`는 8개 LED logical OFF 초기화 후 자체 daemon을 실행한다.
- 버튼을 놓아 재무장한 뒤 다음 유효 누름에 한 번 토글한다. 20ms 주기/3샘플은 IRQ가 아니며 짧은 펄스 인식을 보장하지 않는다.
- 앱 코드에 등장하는 `poll()`은 주로 제어 소켓과 샘플링 주기를 다루며 **GPIO 인터럽트 구현을 뜻하지 않는다**.
- 원본 `all toggle stop`의 전체 LED OFF 정리 동작과 재시작 흐름을 그대로 활용한다.

---

## 4. 저장소 구성 및 Layer 적용 정책

결과 저장소에 필요한 **통합용 파일과 문서만** 추가한다. 아래는 목표 논리 구조이며, BitBake가 실제 참조하는 기존 Layer 구조를 먼저 비교한 후 중복 없이 적용한다.

```text
yocto-rpi4-lab/
├── README.md                                # 기존 결과 보존, 프로젝트 항목 추가
├── configs/
│   └── source-revisions.md                  # 원본/Layer/BSP/커널 commit, 설정 출처
├── layers/
│   └── meta-yocto-lab/
│       ├── conf/layer.conf
│       ├── recipes-kernel/lab-gpio/
│       │   └── lab-gpio_1.0.bb
│       ├── recipes-apps/lab-gpio-test/
│       │   └── lab-gpio-test_1.0.bb
│       ├── recipes-bsp/lab-gpio-overlay/
│       │   ├── lab-gpio-overlay_1.0.bb
│       │   └── files/lab-gpio-eight-overlay.dts
│       └── recipes-core/
│           ├── images/yocto-gpio-toggle-image.bb
│           └── lab-gpio-autostart/
│               ├── lab-gpio-autostart_1.0.bb
│               └── files/lab-gpio-init
│       # 필요 시 커널/BSP 부팅 설정을 안전하게 확장하는 .bbappend/설정 추가
└── docs/
    └── final-uart-gpio8-image.md            # 실제 구현·Host 검증·HW 인계 기록
```

**구현상 중요한 제약:** 공개 GitHub 저장소에 `layers/meta-yocto-lab/`를 넣기만 해서는 BitBake가 해당 Layer를 사용하지 않는다. 다음 중 **실제 상황에 맞는 하나의 방법**만 선택한다.

1. 결과 저장소의 `layers/meta-yocto-lab/` 자체를 **활성 `BBLAYERS` 대상**으로 설정한다. 단, 기존 WSL Layer의 레시피·작업물을 빠짐없이 비교·보존한다.
2. 기존 WSL 활성 Layer를 그대로 유지하고 결과 저장소와 **변경 파일을 명시적으로 동기화**한다. 활성 Layer를 두 벌 등록하지 않는다. 동기화 절차와 차이를 문서화한다.

이미 있는 `examples/hello-drv/`, Ch.1/2/3/5 기록, 기존 thermal 관련 소스는 삭제·재작성하지 않는다. 공개 저장소에 개인 WSL 절대 경로, 인증 비밀, 로컬 생성 캐시, 이미지 바이너리를 무분별하게 올리지 않는다.

---

## 5. Recipe·Boot·자동 시작 상세 설계

### 5.1 `lab-gpio` 드라이버 Recipe

- 출발점: 원본 `ch07-platform-gpio/yocto/lab-gpio_1.0.bb`.
- 원본 `driver/lab_gpio.c`, `driver/Makefile`, `include/lab_gpio_uapi.h`는 반드시 **같은 SHA**의 일치하는 복사본 사용.
- `inherit module`을 유지. 타깃의 **실제 빌드 커널**을 이용하고 `uname -r`로 확인할 예정인 버전과 `modinfo -F vermagic`을 대조한다.
- 원본 Recipe의 `SRC_URI`, `${UNPACKDIR}`, `S`, `B`와 헤더 상대 경로가 BitBake에서 실제 통하는지 검증한다. 소스 디렉터리 배열이 다르면 **Recipe의 패키징 경로만** 조정하고 원본 드라이버 로직은 고치지 않는다.
- `git://`를 이용하면 `SRCREV`를 반드시 고정하고 `AUTOREV`는 금지. 단일 repo subdirectory를 가져오는 방법과 실제 파일 위치를 확인한다. `file://` 복사 방식이면 원본 SHA와 복사·갱신 절차를 문서화한다.
- 실제 모듈 패키지 이름은 `oe-pkgdata-util`, Package Manifest 등을 통해 확인한 뒤 이미지에 추가한다. `kernel-module-lab-gpio`라는 **추정 이름만 믿지 말 것**.
- 커널 자동 적재 정책은 init script의 `modprobe` 또는 패키지 autoload 중 **한 가지**로 정한다. 자동 시작 시 실제 로드 순서가 보장되어야 한다.

### 5.2 `lab-gpio-test` 사용자 앱 Recipe

- 출발점: 원본 `ch07-platform-gpio/yocto/lab-gpio-test_1.0.bb`.
- 동일 소스 SHA의 `userspace/lab_gpio_test.c`와 `include/lab_gpio_uapi.h` 사용.
- `${CC}`, `${CFLAGS}`, `${LDFLAGS}` 등 Yocto Cross Compilation 컨텍스트 사용. Host x86-64 실행 파일이 아니라 **AArch64 ELF**를 설치한다.
- 설치 명령은 실제 `${bindir}`의 `lab_gpio_test`를 제공하게 한다.
- 런타임에 8개 장치명과 `/run/lab-gpio` 런타임 디렉터리를 사용할 수 있게 한다. 원본 앱은 제어 디렉터리를 생성할 수 있지만, `/run`이 실제 부팅 시 쓰기 가능한지 확인한다.
- 다음 CLI를 **변경 없이** 유지한다.

```sh
lab_gpio_test all toggle start
lab_gpio_test all toggle status
lab_gpio_test all toggle stop
```

### 5.3 `lab-gpio-overlay` 8채널 Device Tree Recipe

- 소스는 **8채널 활성 overlay 하나** (`lab-gpio-eight-overlay.dts`)다. 공개 원본 disabled 템플릿은 직접 변경하지 않고 사용자 Layer의 **활성 사본**에서 `status = "okay"`를 적용한다.
- 원본 GPIO 번호/극성과 실검증 배선을 대조한다. 노드명 D0 `lab-gpio`, D1~D7 `lab-gpio-d1` ... `lab-gpio-d7`, compatible `yocto-lab,gpio-demo`를 유지한다.
- Yocto 6.0 `inherit devicetree` 등 실제 클래스 기능으로 `.dtbo`를 생성한다. `-@` 심볼/외부 phandle 처리를 검증한다.
- 클래스의 산출물이 `${DEPLOY_DIR_IMAGE}/devicetree/` 아래에 배포되었는지 확인한다.
- WIC Boot 파티션 내부에는 `overlays/lab-gpio-eight-overlay.dtbo`로 들어가야 한다. **RootFS에 `.dtbo`만 설치하는 것은 불충분**하다.
- 이미지 설정에서 `IMAGE_BOOT_FILES`를 **기존 설정에 추가**하되, 실제 WIC 플러그인이 이를 사용하는지 확인한다. 아래는 구조 예시일 뿐 현재 환경에 바로 강제하지 말 것.

```bitbake
IMAGE_BOOT_FILES:append = " devicetree/lab-gpio-eight-overlay.dtbo;overlays/lab-gpio-eight-overlay.dtbo"
```

- 중요한 점: `IMAGE_BOOT_FILES`는 파일 선정만 담당한다. **새 빌드 트리에서도 WIC 이전에 `.dtbo`가 생성되도록** 이미지와 overlay 배포 Task의 의존성을 연결하고 실제 task 그래프/로그를 확인한다. 예를 들어 현재 클래스가 허용하는 경우 `do_image_wic[depends] += "lab-gpio-overlay:do_deploy"` 같은 방식의 적합성을 검사한다.
- Overlay 컴파일 성공 ≠ Live DT 적용 성공. WIC의 `config.txt`, `.dtbo`, 실제 HW Live DT를 별도로 검증한다.

### 5.4 Raspberry Pi Boot 파티션 / UART

- 실제 `meta-raspberrypi` checkout에서 **`config.txt`를 생성하는 Recipe**, `RPI_EXTRA_CONFIG` 소비 위치, `cmdline.txt` 작성 경로, WIC Boot 파티션 생성 방식을 추적한다.
- 이미지 Recipe 안에서 `RPI_EXTRA_CONFIG`만 설정하면 **다른 Recipe의 task에는 전파되지 않을 수 있음**. 필요하면 Layer 안의 제한적인 `.bbappend` 또는 그 BSP가 지원하는 공유 설정 사용. 어떤 설정이 어느 Recipe에 적용되었는지 `bitbake -e`로 확인한다.
- 기존 `config.txt` 항목은 유지한 채 활성화된 적절한 Section에 다음 설정이 존재하도록 한다.

```ini
enable_uart=1
dtoverlay=lab-gpio-eight-overlay
```

- UART 콘솔을 사용하기 위해 실제 `cmdline.txt` 한 줄의 기존 root/bootargs를 보존하고, 기존 실검증 방식과 동등한 `console=serial0,115200`을 포함한다. 다른 충돌하는 console 설정, 순서 및 타깃의 로그인 getty를 확인한다.
- 현재 이미지가 실제로 Pi Firmware → Linux인지, U-Boot가 DTB를 교체하는지 확인한다. U-Boot가 firmware의 FDT를 대체한다면 overlay의 적용 방식도 재검토한다.
- 기존 단일 D0 overlay와 8채널 overlay가 **동시에 활성화되지 않도록** 한다. 활성 설정 행 및 중복 노드는 이미지 안의 Boot 파일 기준으로 검증한다.
- **WIC를 읽기 전용으로 검사**해 실제 FAT Boot 파티션에 `config.txt`, `cmdline.txt`, `overlays/*.dtbo`가 포함됐는지 증명한다. 빌드 디렉터리의 중간 산출물만 확인하고 완료로 판단하지 않는다.

### 5.5 자동 실행 Recipe / init script

- 먼저 `VIRTUAL-RUNTIME_init_manager`, 배포판 구성, 설치된 `/sbin/init` 및 getty를 확인한다. **기존 SysVinit이면 `inherit update-rc.d`** 기반 `/etc/init.d/lab-gpio`를 사용하고 확인 없이 systemd로 바꾸지 않는다.
- 원본 앱은 `start` 시 자체 fork/daemon + ready handshake를 구현한다. 스크립트에서 `lab_gpio_test ... start &`처럼 다시 백그라운드화하거나 별도 polling daemon을 만들지 않는다.
- 부팅 순서: `/dev`, `/run` 준비 → 모듈 적재(한 가지 autoload 정책) → 8개 장치 노드 유한 재시도 → 원본 `all toggle start` **한 번 호출** → 모든 채널 정상 동작 status 확인.
- `modprobe` 명령이 실제 최소 이미지에 존재하는지 확인하고, 없으면 필요한 모듈 로더의 최소 의존성만 추가하거나 검증된 대안을 사용한다. 실행 실패를 숨기지 않는다.
- 재시도는 **유한한 시간**이어야 한다. 예시 `100ms × 최대 50회`는 약 5초 상한의 설계값일 뿐, 실제 구현의 횟수·간격·timeout을 명시하고 검증한다. 무한 대기 금지.
- 모듈 또는 장치 중 하나라도 준비되지 않으면 시작 실패를 명확히 반환하고 부팅 로그/로깅에 남긴다. 컨트롤러 중복 시작 금지.
- **중요: 원본 `lab_gpio_test all toggle status`는 채널이 stopped여도 정상 종료 코드 0을 반환할 수 있다.** 따라서 **exit code만으로 running 판정 금지**. D0~D7 각 채널의 출력에서 `Toggle controller running.` 상태가 **8개 모두**인지 구분·검사한다. 단순히 `running` 문자열 한 줄만 찾는 것도 불충분하다.
- `stop`: 원본 `lab_gpio_test all toggle stop`를 사용해 모두 OFF 후 종료 여부 확인. `status`: 8개 채널을 점검해 **모두 running일 때만 init status 성공**을 반환하도록 구현한다. `restart`: stop을 먼저 완료한 뒤 단일 start.
- 실패 시에도 UART getty/login은 독립적으로 계속 사용 가능해야 한다.

### 5.6 Custom Image Recipe와 인증 정책

- 새 `yocto-gpio-toggle-image.bb`를 작성해 `core-image-minimal`의 필수 구성 위에 사용자 기능만 추가한다. 기존 이미지의 부팅 필수 패키지 및 커널/DTB는 유지한다.
- **명시적 사용자 설치 항목**은 드라이버의 실제 모듈 패키지, C 앱, autostart Recipe를 중심으로 한다. `.dtbo`는 FAT Boot 파티션을 통해 포함한다. init/getty/모듈 적재에 부족한 필수 의존성만 사유를 기록하고 추가할 수 있다.
- 이미지는 **비밀번호 없는 root 로그인 정책을 사용하지 않는다**. 기존 개발용 `root-login-with-empty-password` Fragment 등은 이 이미지에서 적용되지 않게 한다. 불필요하게 기존 일반 실습 이미지를 파괴하지 않는다.
- UART 로그인 가능한 비어 있지 않은 암호 인증을 구현한다. Yocto `extrausers` 등 지원 방법을 사용하되 **평문 암호는 물론 고정 공개 비밀번호 해시도 GitHub에 커밋하지 않는다.** 로컬 Git-ignored 보안 설정/개별 생성 비밀 사용 방식을 마련한다. 비밀이 필요한 경우 안전한 로컬 생성·보관 위치와 사용자 인계 방법을 보고한다. 비밀 자체를 빌드 로그·README·공개 저장소에 출력하지 않는다. 사용자에게 전달되지 않는 비밀번호로 로그인 불가능한 이미지를 만들고 완료라고 해서는 안 된다.
- 원본 드라이버는 `misc.mode = 0600`이다. Root init script는 접근 가능하나 일반 사용자 계정 운영 시 장치 권한 정책이 필요하다. 이번 범위에서는 인증된 UART 관리 셸과 자동 시작의 실제 권한이 일치하는지 검증한다.
- `IMAGE_FSTYPES`와 실제 결과를 확인해 WIC 산출물을 생성한다. `.wic`, `.wic.bz2`, `.wic.bmap` 등의 구체 파일명은 **실제 생성된 값**을 사용한다.

---

## 6. Codex 실제 실행 순서

### Phase 0 — 읽기 전용 현황 조사

1. 로컬 저장소 경로, `git status --short`, `git remote -v`, `git rev-parse HEAD`, 관련 커밋 SHA 조사.
2. 기존 WSL Yocto `build/init-build-env`, `build/conf/*`, 활성 `BBLAYERS`, `meta-yocto-lab` 실체, Kernel Recipe, BSP Revision, Init Manager, 이미지 포맷 파악.
3. 사용자 실검증 GPIO 버전과 공개 원본 소스·Overlay 비교. 후보 8채널 배선, UART GPIO 충돌 검토.
4. 실제 기존 빌드 캐시/아티팩트가 존재하는지 점검. 별도 Yocto 환경 생성 금지.
5. **변경 대상 파일 목록, 백업 위치, 기준 SHA를 먼저 기록**. 기존 파일·사용자 작업을 지우지 않는다.

### Phase 1 — 소스·Layer/Recipe 구현

1. 단일 활성 Layer 정책 정리, 실제 `BBLAYERS` 적용.
2. Module/App Recipe 원본 재사용 및 BitBake 구조에 맞게 최소 수정.
3. 활성 8채널 `.dts` 사본과 Overlay Recipe 작성.
4. 실제 init 방식에 맞는 autostart Recipe와 `/etc/init.d/lab-gpio` 또는 기존 init manager에 맞춘 서비스 작성.
5. Image Recipe 및 필요한 부팅 설정 확장 추가.
6. 기존 README·설정 문서 최소 수정 및 `docs/final-uart-gpio8-image.md` 작성.

### Phase 2 — 실제 빌드

BitBake Build Environment를 로드한 뒤 실제 환경에서 실행한다. 예시:

```sh
cd ~/study/rootfs/yocto
source bitbake-builds/poky-wrynose/build/init-build-env
pwd
bitbake-layers show-layers
bitbake -e yocto-gpio-toggle-image | grep -E '^(MACHINE|DISTRO|IMAGE_FSTYPES|IMAGE_BOOT_FILES|VIRTUAL-RUNTIME_init_manager)='

bitbake lab-gpio lab-gpio-test lab-gpio-overlay lab-gpio-autostart
bitbake yocto-gpio-toggle-image
ls -lh tmp/deploy/images/raspberrypi4-64/
```

이 명령의 경로는 기존 센터 환경에서 실제 파일 존재를 확인한 후 사용한다. BitBake 오류가 나면 **해당 Recipe/Task 로그를 읽고 원인 최소 수정 후 재시도**한다. 무조건 `clean`, `cleansstate`, `cleanall` 또는 전체 재빌드를 실행하지 않는다. 이미 성공한 Task를 활용한다.

### Phase 3 — Host 정적 검증

1. BitBake `ERROR` 없는 성공 요약, 실패/경고 내역과 복구 과정 기록.
2. `readelf -h`로 앱의 AArch64 ELF, `.ko`의 ARM64 형식 확인.
3. `modinfo -F vermagic` 및 빌드 커널 version/ABI 일치 확인. 실제 패키지명이 정확히 이미지에 포함되었는지 manifest와 pkgdata에서 확인.
4. Overlay `.dtbo`의 8개 노드/호환 문자열/극성/`okay`/중복 여부 검사, 부팅 파티션의 실제 `.dtbo`와 생성 파일 대조.
5. `.wic`(압축돼 있다면 검증 후 읽기 전용 해제) 파티션 테이블·FAT Boot 파티션 확인. `config.txt`, `cmdline.txt`, `overlays/lab-gpio-eight-overlay.dtbo`가 **완성 이미지 안에** 있는지 확인.
6. RootFS에 앱, 모듈, init script 실행 권한, init 등록, 모듈 적재 의존성, UART getty/로그인 설정, `/run` 사용 가능 구성 확인.
7. 실제 init script의 `start`, `stop`, `status` 로직과 8개 채널 running 판정·재시도 한도·중복 시작 방지를 소스/Host 검사로 확인. 실제 daemon 상태는 HW 전 검증했다고 주장하지 않는다.
8. **실제 WIC의 SHA-256**, byte 크기, 압축 포맷, Manifest, 원본/Layer 커밋 목록 기록.

실패하면 결과를 `BLOCKED` 또는 `FAILED`로 정확히 보고하고, 이미 빌드·확인된 부분만 PASS로 표기한다.

### Phase 4 — 사용자 인계 (Codex 작업 종료)

- 실제 WIC(또는 압축 이미지)의 **절대 경로**, 파일명, 크기, SHA-256 제공.
- 이미지 내부 FAT Boot/RootFS 검증 요약과 증빙 로그·문서 경로 제공.
- SD 카드 기록 전에 **Windows에서 기록 대상 장치의 용량·식별자 확인 절차** 제공. `dd`, Raspberry Pi Imager 등 **기록 자체는 실행하지 않는다**.
- UART(115200 8N1), 로그인 인증 정보의 로컬 확인 방식, 8채널 장치명, `start/status/stop`, `dmesg` 등 HW 검증 명령 제공.
- 기존 이미지 복구 방법을 제시한다. 해당 카드의 기존 데이터가 지워질 수 있음을 명시하고 사용자의 명시적 선택을 기다린다.
- Git 변경사항·untracked 파일·사용자에게 남은 TODO를 정리한다. **commit/push 금지**.

---

## 7. Host 검증 증거 체크리스트

| 검사 | 반드시 남길 근거 |
| --- | --- |
| 빌드 메타데이터 | 활성 Layer 및 SHA, `MACHINE`, `DISTRO`, Image Recipe, init manager |
| Recipe 빌드 | 4개 구성 Recipe와 Custom Image 실제 Task Summary |
| APP | `readelf -h` AArch64 / 설치 파일 위치 |
| Module | 실제 `.ko`, ELF architecture, `modinfo -F vermagic`, 모듈 package 이름 |
| Boot/FAT | `.wic` FAT 파티션 `config.txt`, `cmdline.txt`, `overlays/lab-gpio-eight-overlay.dtbo` |
| DT | D0~D7 8개 활성 노드, 올바른 GPIO offset·극성, 중복·충돌 없음 |
| RootFS | 앱/모듈/init 파일 및 실행 권한, getty, 인증 정책, `/run` 환경 |
| Autostart | 모듈 로드 방법 1개, 8노드 유한 대기, 단 한 번 앱 시작, 전체 status 판정 |
| 이미지 재현성 | WIC 경로, 크기, SHA-256, manifest, Layer/source revision |
| 안전성 | Git commit/push 실행 없음, SD/USB 기록 없음, 기존 파일 변경 이력 |

**금지:** Host에서 `lab_gpio_test` 명령의 컴파일·텍스트·exit 상태만 검사한 뒤 실제 GPIO 동작 또는 Pi 부팅을 PASS로 기록하는 것.

---

## 8. Raspberry Pi 4B 실장비 수락 테스트 — 사용자 수행

> 이 단계는 Codex가 수행하지 않는다. 사용자 실장비 관찰 로그를 받은 뒤만 `HW VERIFIED` 처리한다.

| ID | 사용자 시험 | PASS 기준 |
| --- | --- | --- |
| HW-01 | SD에 새 WIC 기록 후 Pi 부팅 | UART 115200 8N1에서 Linux 로그 및 로그인 가능 |
| HW-02 | 부팅 후 수동 명령 없이 상태 확인 | 8개 노드와 모듈 bind, 8채널 컨트롤러 running |
| HW-03 | 자동 시작 직후 상태 관찰 | LED 8개 모두 OFF |
| HW-04 | D0~D7 버튼 각 1회 유효 누름 | 대응 LED 하나만 ON, 다른 LED 불변 |
| HW-05 | D0~D7 버튼 각 1회 더 누름 | 대응 LED 하나만 OFF, 다른 LED 불변 |
| HW-06 | UART에서 `lab_gpio_test all toggle status` | D0~D7 모두 running |
| HW-07 | 정상 재부팅 | UART 로그인·자동 시작·8채널 토글 재현 |
| HW-08 | UART에서 `lab_gpio_test all toggle stop` | 컨트롤러 중지, 모든 LED OFF, 필요 시 수동 재시작 확인 |

추가 관찰: `uname -r`, `ls /dev/lab_gpio-*`, `dmesg`의 8개 probe/bind, `/proc/device-tree` 또는 `/sys/firmware/devicetree/base`의 실제 노드, init status를 기록한다. 버튼 누름/해제는 원본 20ms sampling과 3샘플 debounce에 충분한 시간 동안 유지하여 검사한다.

**상태 표현:**

- `SOURCE INSPECTED`: 원본 SHA 및 적용 설정 확인.
- `RECIPE BUILT`: 개별 Recipe 성공.
- `IMAGE BUILT`: WIC 이미지 생성 성공.
- `HOST VERIFIED`: 이미지·manifest·ABI·Boot/RootFS 정적 검증 완료.
- `HW VERIFIED`: **새 WIC**를 설치한 Pi 실장비에서 HW-01~HW-08 완료.

기존 GPIO 8채널 실증이 있었다는 사용자 기록과 **새 Yocto WIC의 실장비 검증**은 별개로 유지한다.

---

## 9. 산출물·문서 및 보고 형식

### 9.1 반드시 남길 파일

1. `meta-yocto-lab`의 Driver/App/Overlay/Autostart Recipe, 필요한 최소 부팅 확장 파일.
2. `yocto-gpio-toggle-image.bb` Custom Image Recipe.
3. **WIC 이미지**(실제 포맷/압축 여부 그대로), 가능하면 `.wic.bmap`, **SHA-256**.
4. 해당 이미지 Package Manifest, Task/빌드 로그, 커널/애플리케이션/Overlay 검사 결과.
5. `configs/source-revisions.md`의 실제 commit SHA와 활성 Layer·설정 정보.
6. `docs/final-uart-gpio8-image.md`의 설치·검증·문제 해결·복구 절차.
7. 기존 README의 프로젝트 설명과 결과 문서 링크.

일반 Git 저장소에는 **Recipe·설정·소스·문서**를 보관한다. 대용량 WIC, 빌드 캐시·원시 빌드 디렉터리는 별도 로컬 산출물로 보관하고 gitignore 여부를 확인한다. **실제 git commit/push는 사용자가 수행**한다.

### 9.2 Codex 답변 서식

아래 항목을 빠짐없이 **실제 관찰한 값**으로 보고하라.

```text
1. 작업 기준: Host/Build 경로, Git SHA, 사용한 Layer/Recipe
2. 변경 파일: 신규/수정/삭제 경로, 각 변경 이유
3. 구현 결과: Driver/App/Overlay/Autostart/Custom Image
4. 빌드 결과: 각 Recipe와 WIC BitBake PASS/FAIL, Task summary
5. Host 검증: ARM64 ELF, module vermagic, manifest, WIC Boot/RootFS 검사
6. 동작 정책: 8개 장치명, 자동 시작/중지/status, 유한 재시도 횟수와 시간
7. 이미지 산출물: 실제 절대 경로, 파일 크기, SHA-256, 패키지 목록 경로
8. 인증/SD 기록: 비밀번호의 비공개 인계 방식, 사용자 실행 단계·주의사항
9. 미검증/이슈: 차단 사유, HW 미검증 항목, 잔여 위험
10. Git 상태: 변경 내역만 보고, commit/push/SD write 미실행 확인
```

**완료 후 사용자에게 필요한 한 줄:** `빌드 완료 여부 / 이미지 경로 / SHA-256 / 사용자 SD 기록·HW 검증 준비 상태`.

---

## 10. 변경 통제 및 절대 금지 사항

- 실제 8채널 회로/극성/핀 매핑을 검증 근거 없이 변경하지 않는다. GPIO14/15 UART 사용을 방해하지 않는다.
- 기존 SD·USB·SSD/Windows 디스크에 쓰는 `dd`, Imager, 포맷·파티션 명령 실행 금지.
- 원본 GPIO C 드라이버, 앱, UAPI의 동작 로직 변경 금지. 빌드상의 문제가 원본 수정 없이 해결되지 않으면 실패 로그와 최소 수정안을 보고한다.
- 기존 Yocto 구성이나 root 로그인 정책을 **다른 실습 이미지까지** 무차별 변경하지 않는다.
- 실물 장치에 없는 테스트 기능, GPIO IRQ, Thermal Driver, SSH/웹 서비스, GUI, 불필요한 package 추가 금지.
- `AUTOREV`, 근거 없는 패키지명, 임의의 kernel version 고정, 무조건 전체 cleansstate/cleanall 금지.
- 토글 8채널 정상 동작을 Host 정적 검증만으로 `HW VERIFIED`로 포장하지 않는다.
- 사용자 승인 없이 `git commit`, `git push` 또는 기존 원격 작업 내용 변경 금지.

---

## 11. 원본 및 공식 참고 자료

- GPIO 원본 고정 SHA: https://github.com/triton0305/linux-system-programming/tree/56a431d1e633d90f1bfe98a3a7bfaa026494862c/ch07-platform-gpio
- 결과 저장소: https://github.com/triton0305/yocto-rpi4-lab
- Yocto 6.0 `devicetree`, `module`, `update-rc.d`, `extrausers` 클래스: https://docs.yoctoproject.org/6.0/ref-manual/classes.html
- Yocto 6.0 변수 (`IMAGE_BOOT_FILES`, `IMAGE_FSTYPES` 등): https://docs.yoctoproject.org/6.0/ref-manual/variables.html
- `meta-raspberrypi` 추가 부팅 설정 (`RPI_EXTRA_CONFIG`): https://meta-raspberrypi.readthedocs.io/en/latest/extra-build-config.html
- Raspberry Pi Boot/Device Tree 설정: https://www.raspberrypi.com/documentation/computers/configuration.html

---

## 12. 이 작업의 종료 조건

**Codex 단계 종료:** 접근 가능한 기존 WSL Yocto 환경에서 코드/Recipe를 실제 구현하고, WIC 이미지 빌드 및 Host 정적 검증을 수행한 뒤 경로·SHA-256·검증 로그를 보고한다. 불가능한 부분은 원인과 차단 상태를 구분한다. **commit/push와 SD 기록을 하지 않는다.**

**프로젝트 종료:** 사용자가 이미지 하나만 SD에 기록해 Raspberry Pi 4B에서 부팅하고, UART 로그인과 8채널 독립 토글이 자동 시작되며, 재부팅·정지까지 성공한 것을 실제로 확인하고 기록한다.
