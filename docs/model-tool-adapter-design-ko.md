# PosAgent 모델 및 도구 어댑터 설계안

## 1. 목적과 현재 상태

이 문서는 PosAgent 그래프에 모델 호출과 도구 실행을 연결하는 경계를 정의한다. 모델 공급자별 HTTP/SDK 구현은 그래프 코어와 분리하고, 모델이 요청한 도구는 호스트가 등록한 도구만 실행하도록 한다.

현재 코드에는 OpenAI 호환 Chat Completions 어댑터와 Windows WinINet HTTPS 전송 경로가 있다. 실제 endpoint 왕복은 아직 검증되지 않았다. `posagent_context_run_agent()`는 model callback에 상태, 허용 도구 정보, 직전 도구 결과를 전달하고 final 또는 단일 tool call을 처리한다. 기존 `posagent_tool_invoke()`는 별도의 typed C callback API로 계속 제공된다. 구현 계약은 [어댑터 설명서](posagent-chat-adapter-ko.md)를 참고한다.

## 2. 목표 실행 흐름

```text
호스트 상태 / 대화 기록
        |
        v
모델 노드 -- 정규화된 최종 응답 --> 그래프 종료
        |
        +-- 도구 요청 --> 등록 도구 검색 --> 인자 검증 --> 도구 콜백
                                                   |
                                                   v
                                     다음 callback의 last_tool_result
                                                   |
                                                   +--> model callback 재실행
```

첫 구현은 동기식, 단일 실행 문맥으로 제한한다. 비동기 스트리밍, 병렬 도구 실행, 영속 대화 저장은 이 설계의 범위 밖이다.

## 3. 책임 경계

### 그래프 코어

- 실행 상태와 최대 단계 수 관리
- 노드 실행 및 결과에 따른 전이
- 도구 호출 요청을 등록된 도구 실행기로 전달
- 종료/오류 상태와 trace event 제공
- HTTP, TLS, JSON provider envelope를 직접 처리하지 않음

### 모델 어댑터

- 공급자별 요청 형식으로 변환
- 네트워크 호출, 타임아웃, 취소 처리
- 공급자 응답을 PosAgent 공통 응답으로 정규화
- API 키와 공급자 세부 정보를 코어 밖에서 관리

### 호스트 애플리케이션

- 도메인 상태와 대화 기록 소유
- 사용 가능한 도구 명세 및 권한 결정
- 실제 C/C++ 업무 함수를 도구 콜백으로 연결
- 쓰기/제어성 작업에 대한 승인과 안전 정책 유지

## 4. 공통 모델 계약

현재 model callback 요청은 다음 항목을 제공한다.

- 호스트 소유 상태 포인터
- 등록 도구 이름, 설명, 인자 schema 목록
- 직전 tool 결과와 turn index

현재 core callback API는 대화 transcript/system prompt/timeout/cancel을 별도 타입으로 제공하지 않는다. 호스트는 `posagent_chat_config_t`의 history로 텍스트 기록을 제공하고 어댑터가 이를 공급자 request로 변환한다.

모델 어댑터는 다음 결과 중 하나를 반환한다.

- `FINAL`: 사용자에게 반환할 텍스트 응답
- `TOOL_CALL`: 도구 이름 하나와 JSON 인자 하나 (현재 구현 범위)
- `ERROR`: 공급자 오류, 형식 오류, 타임아웃 등

응답에는 provider request ID 등 진단 메타데이터를 선택적으로 둘 수 있다. 비밀 키, 원문 대화, 도구 인자 전체를 기본 trace에 기록하지 않는다.

## 5. 도구 명세와 실행 계약

도구 명세는 고유 이름, 설명, 인자 schema, callback, 사용자 데이터를 가진다. 런타임은 tool 이름을 등록부에서 찾고 입력/출력 크기를 제한한다. 현재 런타임은 [제한된 JSON 스키마](tool-argument-validation-ko.md)를 검증하며, callback이 업무 규칙과 권한을 검증해야 한다.

등록한 문자열과 `user_data`는 graph가 사용하는 동안 유효해야 한다. model request의 tool 목록과 `last_tool_result`의 포인터는 model callback 실행 중에만 유효하며 callback이 보관하면 안 된다.

- 도구가 등록되어 있고 현재 요청에서 허용되었는가
- 런타임이 JSON 문법과 지원 스키마의 필수 필드/type을 검증하는가
- callback이 범위와 업무 규칙을 검증하는가
- 쓰기/제어 도구의 권한을 host policy가 제한하는가
- 입력/출력 버퍼 크기를 넘지 않는가

콜백 결과는 성공/실패 상태와 JSON 결과를 반환한다. 입력/출력 버퍼의 소유권은 명시적으로 정하고, 첫 ABI에서는 호출자가 제공한 버퍼와 용량을 사용하는 방식이 적절하다. 출력 버퍼가 부족하면 부분 JSON을 반환하지 말고 용량 오류로 실패해야 한다.

## 6. 왕복 처리 규칙

1. 모델 callback이 host state, 등록 도구 목록, 직전 tool 결과를 받는다. 대화 이력은 host가 state/user_data 안에서 관리한다.
2. 어댑터 응답을 정규화하고 action 및 필수 필드를 검증한다.
3. `FINAL`이면 상태에 최종 결과를 저장하고 종료 경로로 이동한다.
4. `TOOL_CALL`이면 도구 이름을 등록부에서 확인한다.
5. 런타임이 인자 구문과 스키마를 검증한 뒤 등록 callback을 호출한다.
6. 결과를 다음 model callback의 `last_tool_result`로 전달한다. 런타임이 이를 별도 대화 저장소에 기록하지는 않는다.
7. 모델 노드로 돌아가 최대 왕복 횟수 안에서 반복한다.
8. 알 수 없는 도구, 잘못된 응답, 도구 실패, 최대 횟수 초과는 명확한 오류 코드로 끝낸다.

현재 API는 한 model response에서 tool call 하나만 반환할 수 있다. Chat 어댑터도 실행당 단일 tool call만 지원한다. 여러 호출 배열, core 대화 transcript 타입, 병렬 실행은 지원하지 않는다.

## 7. 보안 및 운영 기준

- 모델 출력은 신뢰하지 않는다. 이름 조회와 인자 검증을 항상 실행한다.
- 기본 허용은 읽기 전용 도구로 제한한다.
- 설비/MES 변경, 명령 전송 등 쓰기 도구는 별도 allowlist와 호스트 승인 단계를 요구한다.
- 모델에 전달하는 상태는 필요한 필드만 선별한다.
- trace에는 실행 ID, 도구명, 결과 코드, elapsed time을 남기되 비밀값과 민감 인자는 마스킹한다.
- 시간 측정은 CPU 시간 `clock()`이 아닌 플랫폼 중립적인 단조 시간 추상화로 전환한다.
- 모델 호출과 도구 호출의 timeout 및 취소는 실제 transport/콜백이 협조해야 한다. 동기 callback을 단순히 감싸는 것만으로 강제 중단이 보장되지는 않는다.

## 8. 첫 통합 PoC 성공 기준

- mock model callback이 `TOOL_CALL`과 `FINAL` 응답을 순서대로 반환한다.
- 모델 요청 -> 등록부 조회 -> 도구 검증 -> 콜백 실행 -> 결과 메시지 반영 -> 최종 응답의 전체 왕복을 테스트한다.
- 미등록 도구와 스키마에 맞지 않는 입력은 callback 전에 거절하고, callback이 거부한 업무 인자는 오류로 전파되는지 확인한다. 범용 JSON Schema는 제공하지 않는다.
- mock 기반 통합 테스트는 인터넷과 API 키 없이 반복 가능하다.
- 실제 모델 어댑터는 같은 공통 계약을 구현하고, 별도 선택적 통합 테스트로 검증한다.

## 9. 단계별 구현 경계

1. 완료: 등록 도구 dispatch와 mock model callback contract
2. 완료: 단일 tool call loop, max turns, 핵심 오류 경계 테스트
3. 완료: Chat Completions 어댑터, 호스트 소유 history mapping, Windows HTTPS 경로의 빌드와 모의 전송 테스트
4. 다음: 실제 endpoint 통합 검증, 서비스 호스트용 WinHTTP 전송
5. 이후 결정: 스키마 부분집합 확장, multiple calls, timeout/retry

LangGraph 기능을 한 번에 복제하지 않는다. 우선 목표는 단일 호스트에서 안전한 모델-도구 왕복이 동작하는 것이다.
