# PosAgent 현재 상태 체크리스트

## 1. 현재 판정

- 상태: mock model -> registered tool -> model 왕복 PoC 검증 완료
- 기준: Windows 샘플 빌드 및 실행이 실제로 통과됨
- 범위: 그래프 실행, mock model 요청, 등록 도구 디스패치, tool 결과 전달, 최종 응답 및 경계 오류 테스트
- 결론: mock 기반 에이전트 왕복은 동작한다. 외부 LLM 연결과 범용 JSON Schema 검증은 아직 없다

> 이 문서는 “기대치”가 아니라 “실제로 확인된 동작” 기준으로 작성한다.

---

## 2. 실제 검증된 기능

- [x] 그래프 생성
- [x] 노드 등록
- [x] 시작 노드 설정
- [x] 일반 전이
- [x] 조건부 전이
- [x] 상태 객체 전달 및 노드에서 갱신
- [x] 모델 callback에 등록 도구 이름/설명/schema 전달
- [x] 도구 등록 시 지원 스키마 검증, 호출 전 JSON 구문/인자 구조 검증
- [x] 모델 tool call을 등록부에서 이름으로 조회 및 dispatch
- [x] tool callback 결과를 다음 모델 turn에 전달
- [x] mock model -> tool -> model 왕복 및 final 응답
- [x] 중복 이름, 미등록 도구, callback 거부 인자, turn 제한, final buffer 부족, model timeout 처리
- [x] 추적 콜백 실행
- [x] 정상 종료 코드와 메시지 출력
- [x] Windows 샘플 빌드 및 실행 성공

주의: 현재 모델은 외부 LLM이 아닌 결정적 mock callback이다. 런타임의 JSON 검증은 [제한된 스키마 부분집합](tool-argument-validation-ko.md)만 지원한다. 업무 규칙과 권한은 각 tool callback이 검증해야 한다. 여러 tool call, 대화 기록 저장/압축, network timeout/cancel도 미구현이다.

---

## 3. 검증 증거

다음 명령으로 마지막 확인을 수행했다.

```powershell
Set-Location "c:/Projects/posagent"; powershell -ExecutionPolicy Bypass -File .\tests\validate_windows_demo.ps1
```

결과:

- exit code: 0
- 최종 메시지: [4/4] Success

추가로 `make -B all test run`에서 독립 테스트와 demo를 실행했다. 이는 mock 왕복과 지정된 오류 경계가 통과했다는 뜻이지 실제 LLM/network 통합을 검증했다는 뜻은 아니다.

WSL Ubuntu에서도 다음 스크립트를 실행했다.

```powershell
wsl bash /mnt/c/Projects/posagent/tests/validate_wsl_demo.sh
```

결과는 exit code 0, `[4/4] Success`, `All PosAgent tests passed`이다.

---

## 4. 아직 미검증 / 이후 확장 필요

- [ ] 외부 LLM provider adapter 및 network 호출
- [ ] 범용 JSON Schema 지원 (현재 제한된 타입/구조 키워드만 검증)
- [ ] 대화 기록 저장 및 다중 메시지 전달 계약
- [ ] 하나의 model response에 대한 여러 tool call 처리
- [ ] 실제 network timeout/cancel 및 retry 정책
- [ ] 오류 경로 추가 자동 테스트 (현재 핵심 경계만 포함)
- [ ] 동적 그래프 생성
- [ ] 병렬 실행 경로
- [ ] 재시도/타임아웃 정책
- [ ] 체크포인트/복원
- [ ] 안전한 tool policy 및 allowlist
- [ ] Linux를 기본 검증 환경으로 승격
- [x] WSL Ubuntu에서 보조 빌드/실행 및 동작 테스트 통과
- [ ] LangGraph급 에이전트 편의 기능 (state graph, memory, stream, retries)

---

## 5. 현실적인 평가

현재 구현은 결정적 mock model과 등록 tool 사이의 동기식 1-call-at-a-time 왕복 PoC다. 실제 provider 연결, 범용 JSON Schema 지원, 대화 이력 정책, 운영용 timeout/retry가 남아 있다.

---

## 6. 다음 올바른 확장 순서

1. provider adapter가 대화 이력과 실제 endpoint를 mock contract에 매핑
2. 현재 스키마 부분집합의 확장 필요성을 실제 도구 요구사항으로 평가
3. 다중 tool call과 운영 오류/timeout 정책 추가
4. Windows 안정화 후 WSL/Linux 보조 검증

이 순서를 지키면 LangGraph처럼 보이기보다, 실제 운영에 필요한 에이전트 런타임으로 성장할 수 있다.

