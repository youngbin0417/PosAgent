# PosAgent 확장 문서 안내

현재 PoC는 Windows에서 mock model -> 등록 도구 -> model 왕복과 제한된 JSON 스키마 인자 검증까지 확인했습니다. OpenAI 호환 Chat Completions 어댑터와 Windows HTTPS 전송 경로가 추가되었으며 실제 endpoint 연동은 아직 검증되지 않았습니다. 범용 JSON Schema 검증도 아직 없습니다.

- [검증 현황 체크리스트](verification-status-ko.md): 실제 동작 확인 항목과 미구현 항목
- [도구 인자 검증 계약](tool-argument-validation-ko.md): 런타임이 지원하는 스키마와 오류 처리
- [Chat Completions 어댑터](posagent-chat-adapter-ko.md): 대화 기록 소유권, Windows HTTPS 전송, 실행 예제
- [모델 및 도구 어댑터 설계안](model-tool-adapter-design-ko.md): 모델 공급자 경계, 등록 도구 dispatch, 검증과 안전 정책
- [에이전트 API 초안](posagent-agent-api-draft-ko.md): 상태 그래프와 모델/도구 왕복을 위한 C API 제안
- [PoC 확장 계획](poc-expansion-roadmap-ko.md): 현재 기준선, 단계별 산출물 및 완료 기준

실제 기능 구현 전에는 API 초안의 ABI, JSON 처리, 메모리 소유권 결정을 먼저 확정해야 합니다.
