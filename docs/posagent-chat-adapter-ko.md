# OpenAI 호환 Chat Completions 어댑터

`posagent_chat_model_callback()`은 기존 `posagent_context_run_agent()`의 모델 콜백 자리에 연결된다. 호스트가 제공한 텍스트 대화 기록과 등록 도구를 Chat Completions 요청으로 바꾸고, 응답의 최종 텍스트 또는 단일 함수 도구 호출을 PosAgent 응답으로 변환한다. 형식은 [OpenAI Chat Completions API](https://developers.openai.com/api/reference/cli/resources/chat/subresources/completions/methods/create)와 [함수 호출 가이드](https://developers.openai.com/api/docs/guides/function-calling)를 따른다.

## 소유권과 수명

- 호스트가 `posagent_chat_message_t` 배열과 문자열, 모델명, endpoint URL, API 키를 소유한다. 이 값들은 `posagent_context_run_agent()`가 끝날 때까지 유효해야 한다.
- 기록의 마지막 메시지는 `user`여야 한다. `system`, `developer`, `user`, `assistant` 역할의 텍스트 메시지만 받는다. 호스트가 실행 사이의 기록 저장과 요약을 결정한다.
- 어댑터는 한 번의 실행 안에서 발생한 도구 호출 ID, 인자, 직전 결과를 임시로 보관한다. 다음 모델 요청에는 assistant `tool_calls` 메시지와 `tool_call_id`가 붙은 tool 메시지를 함께 보낸다. 실행이 끝나면 어댑터를 해제한다. 다음 사용자 요청에는 호스트가 원하는 기록을 다시 구성하고 새 어댑터를 만든다.
- 모델 응답 포인터는 어댑터 내부 버퍼를 가리키며 다음 콜백 호출 또는 어댑터 해제 전까지만 유효하다.

## 범위와 오류

- `parallel_tool_calls: false`로 요청한다. 모델이 한 응답에 둘 이상의 호출을 보내거나 같은 실행에서 두 번째 도구 호출을 보내면 `POSAGENT_ERR_INVALID_RESPONSE`를 반환한다.
- 응답에는 `choices` 항목 하나, `assistant` 역할, `finish_reason`의 `stop` 또는 `tool_calls`가 필요하다. 텍스트와 JSON 응답은 비스트리밍만 지원한다.
- 요청 최대 32767바이트, 응답 최대 65535바이트다. 최종 텍스트와 함수 인자에는 별도 고정 버퍼 한계가 있다. 한계를 넘으면 버퍼/응답 오류로 종료한다.
- Windows 기본 전송은 WinINet HTTPS POST이며 30초 기본 타임아웃을 사용한다. API 키가 있으면 Bearer 헤더로 보낸다. HTTP 오류는 상태 코드를 `result.code`에 담아 `POSAGENT_ERR_INTERNAL`로 반환한다. 응답 본문이나 키는 기본 추적에 넣지 않는다.
- Windows 외 플랫폼에는 기본 전송 구현이 없다. `config.transport`에 호스트 전송 콜백을 지정할 수 있다. [Microsoft는 WinINet을 Windows 서비스에서 사용하지 말라고 안내한다](https://learn.microsoft.com/en-us/windows/win32/wininet/about-wininet). 서비스 호스트는 별도 WinHTTP 전송을 제공해야 하며 WSL/Linux용 실제 전송도 별도로 구현해야 한다.

## Windows에서 실행

```powershell
make chat-demo
$env:POSAGENT_CHAT_ENDPOINT = "https://your-gateway.example/v1/chat/completions"
$env:POSAGENT_CHAT_MODEL = "your-model"
$env:POSAGENT_CHAT_API_KEY = "your-key"
.\build\posagent_chat_demo.exe "안녕하세요"
```

API 키가 필요 없는 호환 게이트웨이는 `POSAGENT_CHAT_API_KEY`를 설정하지 않아도 된다. 실제 endpoint와 계정 정보는 저장소에 기록하지 않는다. `tests/test_posagent_chat.c`에서는 전송 콜백을 대체해 도구 왕복을 검증한다.

## 실제 endpoint 검증

환경변수를 설정한 뒤 `make chat-demo`를 실행한다. 일반 텍스트 응답은 프롬프트 하나를 인자로 전달하고, 도구 왕복은 `--tool`을 전달한다.

```powershell
.\build\posagent_chat_demo.exe "Reply with exactly PONG."
.\build\posagent_chat_demo.exe --tool
```

`--tool`은 `get_validation_marker`를 등록하고 두 번의 모델 요청 사이에 도구 결과를 전달한다. 도구가 정확히 한 번 실행되고 최종 응답에 결과의 표식이 포함될 때만 성공한다. 데모는 느린 endpoint를 고려해 90초 전송 타임아웃을 사용한다.

2026-09-30에 `motif/motif-3`의 실제 Chat Completions endpoint에서 텍스트 응답과 도구 왕복을 확인했다. 인증 정보는 소스와 로그에 저장하지 않았다.
