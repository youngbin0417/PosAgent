# Windows-first 검증 가이드

## 1. 목적

이 문서는 PosAgent의 우선 검증 대상이 Windows임을 명시하고, Linux는 WSL에서 보조 검증하는 절차를 정리한다. 목표는 실제로 실행 가능한 최소 테스트를 가장 먼저 통과시키는 것이다.

현재 확인된 결과는 Windows와 WSL Ubuntu에서의 그래프 정상 경로, mock model -> 등록 tool -> model 왕복, 동작 테스트 통과다. 실제 LLM provider/network와 일반 JSON Schema 검증은 확인하지 않았다.

---

## 2. 검증 전략

### 2.1 우선 순서

1. Windows에서 최소 샘플 코드 빌드
2. Windows에서 실행 결과 확인
3. 추적 로그와 종료 코드 점검
4. WSL에서 동일 소스 빌드 검증
5. 플랫폼 차이를 정리한 뒤 다음 단계로 확장

### 2.2 분리 원칙

- Windows는 기본 검증 환경이다.
- Linux는 WSL로 보조 검증한다.
- OS별 차이는 플랫폼 계층에서 분리한다.
- 기능 검증은 공통 API가 먼저 통과해야 한다.

---

## 3. Windows 실행 절차

### 3.1 PowerShell 기준

```powershell
cd C:\Projects\posagent
New-Item -ItemType Directory -Force .\build | Out-Null
gcc -I include -std=c11 -Wall -Wextra -O2 .\src\posagent.c .\src\posagent_json.c .\examples\posagent_demo.c -o .\build\posagent_demo.exe
.\build\posagent_demo.exe
```

### 3.2 확인 포인트

- 컴파일 경고가 없는지 확인
- 프로그램이 정상 종료되는지 확인
- trace 로그가 출력되는지 확인
- 최종 상태 값이 기대값과 일치하는지 확인

예시 출력:

```text
[TRACE] exec=1 node=1 type=1 msg=start
[TRACE] exec=1 node=2 type=1 msg=check
[TRACE] exec=1 node=3 type=1 msg=end_true
status=0 code=0 message=graph execution completed
final state: value=10 tool_value=15 branch_decision=1
[TRACE] exec=1 node=-1 type=7 msg=model request
[TRACE] exec=1 node=-1 type=8 msg=double_value
[TRACE] exec=1 node=-1 type=3 msg=double_value
[TRACE] exec=1 node=-1 type=4 msg=double_value
[TRACE] exec=1 node=-1 type=7 msg=model request
[TRACE] exec=1 node=-1 type=8 msg=final response
[TRACE] exec=1 node=-1 type=6 msg=agent completed
agent response: The doubled value is 14.
```

---

## 4. WSL Linux 보조 검증 절차

### 4.1 WSL에서 동일 소스 빌드

```bash
cd /mnt/c/Projects/posagent
mkdir -p build
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c examples/posagent_demo.c -o build/posagent_demo
./build/posagent_demo
```

### 4.2 WSL 검증 포인트

- 동일한 소스가 Linux에서도 컴파일되는지 확인
- `clock()` 기반 포팅이 Linux와 Windows 모두에서 동작하는지 점검
- 기능 로그가 동일한 의미를 가지는지 비교

---

## 5. 자동 검증 스크립트

PowerShell 자동 검증:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\validate_windows_demo.ps1
```

WSL 예시:

```bash
cd /mnt/c/Projects/posagent
mkdir -p build
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c examples/posagent_demo.c -o build/posagent_demo
./build/posagent_demo
```

---

## 6. 테스트 시나리오

### 6.1 기본 실행 시나리오

- 그래프 생성
- 시작 노드 실행
- 다음 노드로 이동
- 종료 코드 확인

### 6.2 분기 시나리오

- 조건 기반 경로 전환
- true 경로와 false 경로 분기 확인

### 6.3 도구 실행 시나리오

- 현재 확인: 등록부에서 tool 조회 및 callback 실행
- 현재 확인: tool 결과를 다음 model turn에 전달
- 현재 확인: mock model -> 등록 tool -> model 재호출 -> final response 왕복
- 현재 확인: 제한된 스키마에 대한 도구 인자 사전 검증
- 미확인: generic JSON Schema 검증
- 미확인: 실제 LLM provider/network adapter

### 6.4 실패 시나리오

- 잘못된 노드 접근
- 잘못된 도구 호출
- max_steps 초과

---

## 7. 완료 기준

다음 조건을 충족하면 Windows **기본 그래프 샘플** 검증이 완료로 간주한다. 이는 에이전트/tool 통합 완료를 뜻하지 않는다.

- Windows에서 샘플 코드가 빌드된다.
- 샘플 코드가 정상 실행된다.
- trace 로그가 관측 가능하다.
- 종료 코드와 메시지가 기대와 일치한다.
- WSL Ubuntu에서 동일 demo와 테스트 스크립트가 통과한다.

---

## 8. 설치 권장 사항

- Windows: GCC 또는 MinGW 설치
- WSL: Ubuntu 또는 Debian 설치 후 gcc 설치
- 공통 검증: 동일 코드와 동일 테스트 명령을 사용

필요한 패키지 예시:

```bash
sudo apt update
sudo apt install build-essential
```

---

## 9. 실무 운영 원칙

- Windows에서 먼저 검증한다.
- Linux는 WSL로 보조 검증한다.
- 서로 다른 OS에서 다르게 보이는 결과는 플랫폼 계층에서 정리한다.
- 공통 API와 그래프 동작을 먼저 확인한 뒤, OS별 세부 사항을 확장한다.
