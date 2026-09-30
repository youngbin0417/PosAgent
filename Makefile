CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude
BUILD_DIR := build
EXEEXT :=
ifeq ($(OS),Windows_NT)
EXEEXT := .exe
endif
DEMO := $(BUILD_DIR)/posagent_demo$(EXEEXT)
CHAT_DEMO := $(BUILD_DIR)/posagent_chat_demo$(EXEEXT)
TEST := $(BUILD_DIR)/test_posagent$(EXEEXT)
CHAT_TEST := $(BUILD_DIR)/test_posagent_chat$(EXEEXT)
RUNTIME := src/posagent.c src/posagent_json.c
RUNTIME += src/posagent_chat.c src/posagent_chat_transport.c
LDLIBS :=
ifeq ($(OS),Windows_NT)
LDLIBS += -lwininet
endif

.PHONY: all demo chat-demo test run clean

all: demo $(TEST) $(CHAT_TEST)

demo: $(DEMO)

chat-demo: $(CHAT_DEMO)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(DEMO): $(RUNTIME) examples/posagent_demo.c include/posagent.h include/posagent_chat.h src/posagent_json.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RUNTIME) examples/posagent_demo.c -o $@ $(LDLIBS)

$(CHAT_DEMO): $(RUNTIME) examples/posagent_chat_demo.c include/posagent.h include/posagent_chat.h src/posagent_json.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RUNTIME) examples/posagent_chat_demo.c -o $@ $(LDLIBS)

$(TEST): $(RUNTIME) tests/test_posagent.c include/posagent.h include/posagent_chat.h src/posagent_json.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RUNTIME) tests/test_posagent.c -o $@ $(LDLIBS)

$(CHAT_TEST): $(RUNTIME) tests/test_posagent_chat.c include/posagent.h include/posagent_chat.h src/posagent_json.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RUNTIME) tests/test_posagent_chat.c -o $@ $(LDLIBS)

test: $(TEST) $(CHAT_TEST)
	./$(TEST)
	./$(CHAT_TEST)

run: $(DEMO)
	./$(DEMO)

clean:
	rm -rf $(BUILD_DIR)
