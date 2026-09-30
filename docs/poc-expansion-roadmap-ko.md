# PosAgent PoC 확장 계획

## 1. 목표

현재 Windows에서 확인된 최소 그래프 코어를 출발점으로, mock model과 등록 tool 사이의 제한된 왕복을 구현했다. LangGraph 전체 기능 복제가 목표가 아니다. 다음 질문은 “실제 provider와 기존 C 함수를 제한된 안전한 흐름으로 연결할 수 있는가?”이다.

## 2. 현재 기준선

### 확인된 것

- 그래프 생성, 노드 등록, 시작 노드, 일반/조건부 전이
- 호스트 상태 포인터를 통한 상태 갱신
- trace callback과 최대 단계 코드 경로
- Windows 샘플의 빌드 및 실행 성공
- 등록 도구 이름 조회 및 callback dispatch
- model callback에 tool 정보와 마지막 tool 결과 전달
- mock model -> tool -> model 왕복 및 지정 오류 경계 테스트
- 제한된 JSON 스키마 등록 및 도구 인자 사전 검증

### 아직 확인되지 않았거나 구현되지 않은 것

- 실제 LLM endpoint와의 인증된 network 왕복 (어댑터는 모의 전송으로 검증)
- 범용 JSON Schema 지원 (현재 제한된 타입/구조만 런타임에서 검증)
- 대화 이력 전체의 저장/전달 정책
- 한 응답 내 다중 tool call 처리
- 운영용 timeout/retry/cancel 및 확대된 오류 테스트

현재 판정은 “mock 기반 agent/tool 왕복 PoC 통과”이며, “실제 모델 provider 통합 완료”는 아니다.

## 3. 단계 계획

| 단계 | 범위 | 산출물 | 완료 기준 |
| --- | --- | --- | --- |
| 0. 기준선 정리 | 문서/API/샘플 간 불일치 정리, 체크리스트 정확화 | 상태 문서, 재현 명령 | 검증된 것과 미구현이 구분됨 |
| 1. 코어 신뢰성 | API 반환값 점검, 중복 ID/미등록 노드/start/max_steps/노드 오류 테스트 | 독립 테스트 프로그램 | 정상 및 오류 경로의 status와 메시지 assertion 통과 |
| 2. 실도구 디스패치 | 등록 도구 조회, callback, 결과 전달 | 완료 | unknown tool/인자 실패 테스트 통과 |
| 3. Model contract | provider-neutral callback과 tool metadata 전달 | 완료 | mock callback이 schema metadata와 이전 결과 확인 |
| 4. Agent loop | model -> tool dispatch -> model, max turns | 완료 | round trip, turn 제한, final buffer 테스트 통과 |
| 5. 실제 endpoint 연결 | OpenAI 호환 Chat Completions 어댑터와 실제 gateway 검증 | 어댑터/Windows HTTPS 경로 구현, 실연동 미검증 | 실제 provider가 mock과 동일 contract로 동작 |
| 6. 배포 검증 | Windows 기준 안정화 후 WSL/Linux 보조 검증 | Windows 및 WSL Ubuntu 확인 | 양 환경에서 동일 API 계약과 결과 확인 |

## 4. 단계 1의 필수 테스트

- null 인자와 메모리 할당 실패 처리
- 중복 node ID 및 중복 tool name 거부
- 시작 노드 미설정/미등록 노드로 graph compile 또는 execute 실패
- edge 목적지 미등록 시 검출
- node callback 오류가 최종 result로 전달
- max_steps가 실제로 제한하고 trace/error를 올바르게 남김
- trace callback의 event type 및 순서 확인
- graph/context 생성 실패 시 안전한 정리

현재 구현은 일부 검증을 실행 중에야 하고, 설정 함수 반환값을 데모에서 확인하지 않는다. 단계 1에서 build/compile 단계 검증으로 옮길 항목을 정한다.

## 5. 최소 유용 시나리오

초기 통합 예제는 읽기 전용 업무로 제한한다.

1. 호스트가 허용된 상태 요약만 모델에 전달한다.
2. mock/실제 모델이 `get_status` 도구 호출을 반환한다.
3. PosAgent가 등록 여부와 입력 JSON의 구문/스키마를 확인한다. tool callback이 업무 범위와 권한을 검증한다.
4. C callback이 모의 상태 조회 결과를 반환한다.
5. 결과를 다음 model callback에 전달해 모델을 다시 호출한다. 지속적인 대화 이력은 host가 소유한다.
6. 모델의 최종 응답으로 실행을 마친다.

쓰기/PLC 제어 도구는 첫 PoC에 포함하지 않는다. 이후 추가하더라도 호스트 측 권한 검사와 사람 승인 단계를 먼저 둔다.

## 6. 종료 기준과 변경 통제

- 각 단계는 완료 기준을 만족하고 테스트 로그/명령을 기록해야 다음 단계로 넘어간다.
- 모델 API 사용은 단계 4 mock 통합이 끝난 뒤 시작한다.
- 병렬 노드, stream, checkpoint, persistence, 멀티에이전트, AIX/HP-UX는 이번 확장 범위에서 제외한다.
- API ABI 변경이 필요하면 현재 API를 덮어쓰지 말고 버전 분리 또는 호환성 계획을 먼저 기록한다.
- “지원”은 문서에 적힌 목표가 아니라 실제 빌드와 동작 테스트로만 선언한다.

## 7. 당장 착수할 순서

1. OpenAI 호환 endpoint와 credential 설정을 실제 환경에서 검증한다.
2. conversation history ownership을 결정하고 현재 제한된 JSON 스키마의 확장 필요성을 평가한다.
3. mock contract를 유지하며 provider adapter의 실제 integration test를 추가한다.
4. 필요 시 multiple tool calls 및 timeout/retry 정책을 확장한다.
