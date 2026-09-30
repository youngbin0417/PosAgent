# PosAgent 사용자 설명서

## 1. 개요

PosAgent는 C/C++ 기반 호스트 프로그램에 AI 에이전트 실행 흐름을 연결하기 위한 경량 그래프 런타임을 목표로 합니다. 다음은 목표 기능이며, 모두 현재 구현 완료를 뜻하지는 않습니다.

- 기존 상태 구조체와 함수 재사용
- 그래프 기반 실행 흐름 구성
- 조건부 분기와 반복 제어
- 도구 등록 및 실행
- 추적 로그와 종료 코드 제공

이 문서는 PosAgent를 사용하기 위한 설명서로, 실제로 검증된 Windows 기준 샘플과 함께 정리합니다.

현재 검증 범위는 최소 그래프 정상 실행과 함께, 결정적 mock model -> 등록 tool -> model 왕복입니다. 실제 외부 LLM 연결과 범용 JSON Schema 검증은 아직 없습니다.

---

## 2. PosAgent가 무엇인지

PosAgent는 “AI 모델 자체”가 아니라, AI 실행 흐름을 기존 C/C++ 프로그램 안에 넣는 실행 계층입니다.

즉, PosAgent는 다음 역할을 수행합니다.

- 상태를 읽고 갱신하는 그래프 실행
- 노드 간 전이 관리
- 조건 기반 분기 처리
- 도구 등록 및 실행
- 실행 이벤트 추적
- 오류/종료 코드 전달

핵심은 기존 코드와 재사용 가능하게 만들면서, AI가 어디서 어떤 기능을 호출할지 흐름을 명확히 제어하는 것입니다.

---

## 3. 기본 구성

PosAgent는 다음 구성 요소를 중심으로 동작합니다.

### 3.1 그래프

그래프는 실행 흐름 전체를 정의합니다.

- 시작 노드
- 노드 목록
- 일반 전이
- 조건부 전이
- 최대 실행 횟수

### 3.2 노드

노드는 하나의 작업 단위입니다.

- 상태를 읽는다.
- 내부 로직을 수행한다.
- 결과를 반환한다.

### 3.3 도구

도구는 외부 함수 또는 기존 C 함수를 래핑해서 등록하는 요소입니다.

- 이름
- 설명
- 입력 JSON 계약
- 실행 콜백
- 결과 처리

에이전트 루프의 tool은 `posagent_agent_tool_t`로 등록합니다. 런타임은 [지원하는 스키마 부분집합](tool-argument-validation-ko.md)에 맞는 인자만 콜백에 전달합니다. 업무 규칙과 권한 검사는 각 tool callback이 수행해야 합니다.

### 3.4 실행 컨텍스트

실행 컨텍스트는 한 번의 그래프 실행 상태를 저장합니다.

- 현재 노드
- 실행 횟수
- 추적 콜백
- 종료 상태

---

## 4. 설치 및 준비

### 4.1 Windows 기준

PowerShell에서 다음 명령으로 빌드합니다.

```powershell
cd C:\Projects\posagent
New-Item -ItemType Directory -Force .\build | Out-Null
gcc -I include -std=c11 -Wall -Wextra -O2 .\src\posagent.c .\src\posagent_json.c .\src\posagent_chat.c .\src\posagent_chat_transport.c .\examples\posagent_demo.c -o .\build\posagent_demo.exe -lwininet
.\build\posagent_demo.exe
```

### 4.2 WSL 기준

```bash
cd /mnt/c/Projects/posagent
mkdir -p build
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c src/posagent_chat.c src/posagent_chat_transport.c examples/posagent_demo.c -o build/posagent_demo
./build/posagent_demo
```

> 현재 단계에서는 Windows 검증이 우선입니다. WSL은 보조 검증용으로만 사용합니다.

---

## 5. 가장 작은 예제

아래는 가장 단순한 예제 흐름입니다.

```c
#include "posagent.h"

static void start_node(void *state, void *user_data, posagent_result_t *result) {
    (void)user_data;
    result->status = POSAGENT_OK;
    snprintf(result->message, sizeof(result->message), "start");
}

static void end_node(void *state, void *user_data, posagent_result_t *result) {
    (void)state;
    (void)user_data;
    result->status = POSAGENT_OK;
    snprintf(result->message, sizeof(result->message), "end");
}
```

그리고 그래프를 구성합니다.

```c
posagent_graph_t *graph = NULL;
posagent_context_t *ctx = NULL;

posagent_graph_create(&graph);
posagent_graph_set_max_steps(graph, 8);
posagent_graph_add_node(graph, 1, "start", start_node, NULL);
posagent_graph_add_node(graph, 2, "end", end_node, NULL);
posagent_graph_set_start_node(graph, 1);
posagent_graph_add_edge(graph, 1, 2);
```

실행은 다음처럼 수행합니다.

```c
posagent_result_t result = {0};
posagent_context_create(graph, state, &ctx);
posagent_context_execute(ctx, &result);
```

---

## 6. 상태 모델

PosAgent는 상태를 호스트가 소유하는 방식으로 설계합니다.

```c
typedef struct {
    int value;
    int tool_value;
    int branch_decision;
} demo_state_t;
```

이 구조체는 다음 의미를 가집니다.

- `value`: 기본 상태 값
- `tool_value`: 도구 실행 결과
- `branch_decision`: 분기 결과

호스트는 상태를 직접 관리하며, PosAgent는 그래프 실행 중 해당 상태를 읽고 갱신합니다.

---

## 7. 도구 등록과 에이전트 루프

기존 typed C 도구 API는 callback을 직접 호출하는 용도입니다. 모델 응답에서 도구를 고르고 결과를 다시 모델에 넘기는 흐름에는 agent-tool API를 사용합니다.

```c
posagent_agent_tool_t tool = {
    "status_tool",
    "Returns current status",
    "{\"type\":\"object\",\"properties\":{\"unit\":{\"type\":\"string\"}},\"required\":[\"unit\"]}",
    status_tool_callback,
    user_data
};

posagent_agent_tool_register(graph, &tool);
```

모델 callback은 `posagent_agent_model_request_t`를 받습니다. 요청에는 호스트 상태 포인터, 등록 tool 명세, turn 번호, 직전 tool 결과가 포함됩니다. 첫 호출에서 tool 요청을 반환하고 다음 호출에서 최종 응답을 반환하면 PosAgent가 사이의 tool dispatch를 수행합니다.

```c
char final_response[256];
posagent_context_run_agent(ctx, model_callback, model_user_data,
                           4, final_response, sizeof(final_response), &result);
```

완전한 mock 예제는 [examples/posagent_demo.c](../examples/posagent_demo.c)에서 확인할 수 있습니다. 현재는 응답 하나에 tool call 하나만 허용하며, 대화 이력은 호스트가 상태/user_data로 관리해야 합니다.

---

## 8. 조건부 전이

조건부 전이는 현재 상태를 기준으로 다음 노드를 선택합니다.

```c
static int route_decision_fn(void *state, void *user_data) {
    demo_state_t *demo = (demo_state_t *)state;
    return demo->tool_value > 10;
}
```

이 함수가 `1`을 반환하면 true 노드로 이동하고, `0`이면 false 노드로 이동합니다.

```c
posagent_graph_add_conditional_edge(graph, 2, 3, 4, route_decision_fn, NULL);
```

---

## 9. 추적 로그

PosAgent는 추적 콜백을 통해 실행 흐름을 남길 수 있습니다.

```c
static void trace_callback(const posagent_trace_event_t *event, void *user_data) {
    printf("[TRACE] exec=%llu node=%d type=%d msg=%s\n",
           (unsigned long long)event->execution_id,
           event->node_id,
           event->type,
           event->message ? event->message : "");
}
```

사용 예:

```c
posagent_context_set_trace_callback(ctx, trace_callback, NULL);
```

---

## 10. 결과 처리

실행 결과는 `posagent_result_t` 구조체로 반환됩니다.

```c
posagent_result_t result = {0};
posagent_context_execute(ctx, &result);

printf("status=%d code=%d message=%s\n", result.status, result.code, result.message);
```

현재 정의된 상태는 다음과 같습니다.

- `POSAGENT_OK`: 정상 완료
- `POSAGENT_ERR_INVALID_ARG`: 잘못된 인자
- `POSAGENT_ERR_NOT_FOUND`: 노드 또는 도구 없음
- `POSAGENT_ERR_TIMEOUT`: 타임아웃
- `POSAGENT_ERR_TOOL`: 도구 callback 오류
- `POSAGENT_ERR_MAX_STEPS`: 최대 실행 횟수 초과
- `POSAGENT_ERR_CANCELLED`: 취소
- `POSAGENT_ERR_BUFFER_TOO_SMALL`: 결과 버퍼 부족
- `POSAGENT_ERR_INVALID_RESPONSE`: model 응답 형식 오류

`TIMEOUT`과 `CANCELLED` 코드는 외부 callback이 반환할 수 있는 값입니다. PosAgent가 동기 callback을 강제로 중단하거나 timeout을 자동 적용하지는 않습니다.

---

## 11. 현재 검증된 기능

다음 항목은 현재 샘플 실행에서 실제로 확인했습니다.

- [x] 그래프 생성
- [x] 노드 등록
- [x] 일반 전이
- [x] 조건부 전이
- [x] 등록 tool dispatch
- [x] JSON 구문과 제한된 스키마에 대한 도구 인자 검증
- [x] mock model -> tool -> model 왕복
- [x] 미등록 tool, 잘못된 tool 입력, turn 제한 테스트
- [x] 종료 코드 반환
- [x] 추적 로그 출력
- [x] Windows 샘플 빌드 및 실행 성공

다음 기능은 아직 구현 또는 검증되지 않았습니다.

- [x] OpenAI 호환 Chat Completions 어댑터와 Windows HTTPS 전송 경로 (모의 전송 테스트)
- [ ] 실제 LLM endpoint 네트워크 왕복 검증
- [ ] 범용 JSON Schema validator
- [ ] 대화 이력 저장과 여러 tool call 처리
- [ ] timeout/cancel 상태의 실제 처리 (상태 코드 정의만 존재)
- [ ] Linux 전용 최적화
- [ ] AIX/HP-UX 이식성
- [ ] 병렬 그래프
- [ ] 비동기 스트리밍
- [ ] 체크포인트/복원

---

## 12. 개발자가 주의할 점

### 12.1 상태 소유권

상태는 호스트가 관리합니다. PosAgent는 상태의 참조를 사용하며, 상태 자체를 내부에서 소유하지 않습니다.

### 12.2 도구는 제한적으로 등록

도구는 허용된 이름과 제한된 인자만 사용할 수 있도록 설계하는 것이 안전합니다.

### 12.3 종료 조건 명시

모든 정상 실행과 오류 경로는 종료 코드로 전달되어야 합니다. 그래프는 무한 루프를 방지하기 위해 max_steps를 사용해야 합니다.

---

## 13. 다음 단계

현재 단계의 목표는 다음과 같습니다.

1. mock model/tool 왕복의 오류 경계 추가
2. 실제 endpoint에서 provider adapter의 인증과 응답 호환성 검증
3. 필요하면 JSON Schema 검증 라이브러리 도입
4. 추가 플랫폼 확장은 Windows 안정화 뒤 진행

이 단계가 끝나면, 다음으로는 실제 운영용 API와 테스트 구조를 보완할 수 있습니다.

---

## 14. 결론

PosAgent는 기존 C/C++ 프로그램에 AI 실행 흐름을 연결하기 위한 경량 런타임을 목표로 합니다. 현재 Windows 샘플은 mock model과 등록 tool 사이의 제한된 왕복을 실행하고, 별도 Chat Completions 어댑터는 모의 전송 테스트를 통과했습니다. 실제 LLM endpoint 통합, 일반 JSON Schema 강제, 여러 tool call과 영속 대화 이력은 아직 검증되지 않았습니다.

이 문서는 사용자가 바로 시작할 수 있도록 핵심 개념과 실행 예제를 함께 정리한 설명서입니다.
