# BioSight FAD 제어반 펌웨어

Arduino Mega 2560(ATmega2560) 보드에서 도는 제어반 펌웨어다. 보드에 직접 붙은 입출력을 읽고 쓰면서, 상위 PC의 명령을 받고,
KiSAN 원격 입출력 모듈과 직렬로 통신해 아날로그 입력을 모으고 디지털 출력을 내보낸다.

- 펌웨어 버전: `v3.1.2`
- 개발 환경: AVR GCC 16.1.0(C++23), CMake 4.4.3·Ninja 1.13.2, avrdude 8.3. IDE 없이 명령 줄에서 빌드한다.
- 상위 PC·KiSAN 통신 명령과 프레임 형식은 [통신 프로토콜 기술서](Protocol%20Specification.md)에 있다.

## 무엇을 하는 프로그램인가

제어반 전면의 스위치와 표시등, 밸브 출력을 다룬다. 동작은 제어 모드 세 가지로 갈린다.

| 모드 | 언제 들어가나 | 무엇을 하나 |
|---|---|---|
| 비상 | 비상 입력(입력 15번)이 들어올 때 | 모든 출력을 끄고 경보 출력만 켠다 |
| 수동 | 전면 선택 스위치가 수동일 때 | 전면 버튼을 누르면 짝지어진 밸브 출력이 바로 바뀐다 |
| 자동 | 전면 선택 스위치가 자동일 때 | 상위 PC가 보낸 명령으로만 출력이 바뀐다 |

## 하드웨어 연결

| 구분 | 연결 |
|---|---|
| 디지털 입력 16점 | 핀 30~45 |
| 디지털 출력 8점 | 핀 22~29 |
| 아날로그 입력 4점 | A0~A3 (0~20 mA 범위로 환산) |
| 상위 PC 통신 | 핀 18·19 (`USART1`), 9600 bps |
| 점검용 USB 통신 | USB (`USART0`), 9600 bps. 상위 PC와 같은 명령을 받는다 |
| KiSAN 모듈 버스 | 핀 14·15 (`USART3`), 9600 bps. KM6015 아날로그 입력 8채널, KM6063 디지털 출력 8점 |

## 처음 준비하기

Windows 11에서 저장소 루트를 열고 한 번 실행한다. 필요한 도구를 정해진 버전으로 내려받아 해시를 확인한 뒤 설치한다.

```powershell
python Scripts/verify.py setup
```

설치되는 것은 AVR GCC 16.1.0 묶음과 avrdude 8.3(`%LOCALAPPDATA%\BioSight_FAD\tools`), CMake·Ninja(Python 패키지),
PC 시험용 LLVM(winget), 시뮬레이션용 simavr(WSL Ubuntu 26.04)다. 버전과 해시는 `toolchain.lock.json`에 있다.

## 빌드하기

```powershell
python Scripts/verify.py build
```

Release와 Debug를 동시에 빌드한다. 결과물은 `build/release/FAD.hex`와 `build/debug/FAD.hex`다.
CMake를 직접 쓸 때는 `cmake --preset release`와 `cmake --build --preset release`를 차례로 실행한다. VS Code나 Visual Studio에서 폴더를 열면 같은 프리셋이 보인다.

## 검증하기

```powershell
python Scripts/verify.py all
```

아래 단계를 모두 돌린다. 서로 기다릴 필요가 없는 단계(WSL 준비, 경로 시험, 빌드 두 개, PC 시험)는 동시에 돌고, 결과는 아래 순서대로 보인다.

- `setup` — 도구가 모두 있는지 확인하고 빠진 것만 설치
- `check` — 파일 규칙(UTF-8·LF·170열·주석 없음), 빌드 목록, 버전 표기 대조
- `build` — Release·Debug 빌드, 경고가 하나라도 있으면 실패
- `test` — PC에서 로직 단위 시험 35개
- `simulate` — Release 펌웨어를 simavr로 돌려 응답 기록 369줄을 기대 기록과 대조하고 루프·응답 시간을 잰다
- `paths` — 공백·한글·260자가 넘는 경로에서 빌드와 시험

성능이 바뀌는 수정은 아래 두 명령으로 전후를 잰다. 두 명령은 `all`에 들어 있지 않다.

- `compare --base <판>` — 다른 판(기본은 `HEAD`)과 같은 시나리오로 돌려 크기, 명령별 처리 시간, 루프 시간을 나란히 보인다
- `profile --min-cycles <사이클>` — simavr 명령 주소 표본으로 시간이 드는 함수와 소스 줄을 찾는다

## 보드에 기록하기

보드를 USB로 연결하고 장치 관리자에서 포트 이름(예: COM5)을 확인한 뒤 실행한다.

```powershell
python Scripts/verify.py flash --port COM5
```

`build/release/FAD.hex`를 Arduino 부트로더로 기록한다. 기록이 끝나면 보드가 새 펌웨어로 다시 시작한다.

## 펌웨어를 고칠 때

- 버전의 원본은 `Firmware/include/GlobalDefinition.h`의 `VERSION` 한 곳이다. 버전을 올리면 이 파일 머리말과 프로토콜 기술서의
  `GVER` 예제를 같은 작업에서 함께 고친다. `python Scripts/verify.py check`가 세 곳이 같은지 확인한다.
- 입출력 점수, KiSAN 채널 수 같은 상수도 같은 헤더에 모여 있다. 이 값을 바꾸면 상위 PC 응답의 자릿수와 출력 번호 배치가 함께 바뀐다.
- 하드웨어에 닿는 코드는 `Firmware/src/Board.cpp` 한 파일에만 둔다. 나머지 로직은 PC 시험에서도 그대로 컴파일된다.
- 새 소스 파일은 `CMakeLists.txt`의 목록에 더한다. 목록과 실제 파일이 다르면 `check`가 실패한다.
- 통신 명령이나 응답을 바꾸면 프로토콜 기술서와 호스트 시험을 같은 작업에서 고친다.
- 상태를 가진 것만 클래스로 두고, 상태 없이 한 가지 일만 하는 것은 함수로 둔다. 동적 할당과 기다리는 호출은 쓰지 않는다.
- 빌드 산출물(`build/`)과 검증 산출물(`artifacts/`)은 저장소에 넣지 않는다.
