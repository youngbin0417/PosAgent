CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude
BUILD_DIR := build
EXEEXT :=
ifeq ($(OS),Windows_NT)
EXEEXT := .exe
endif
DEMO := $(BUILD_DIR)/posagent_demo$(EXEEXT)
TEST := $(BUILD_DIR)/test_posagent$(EXEEXT)
RUNTIME := src/posagent.c src/posagent_json.c

.PHONY: all demo test run clean

all: demo $(TEST)

demo: $(DEMO)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(DEMO): $(RUNTIME) examples/posagent_demo.c include/posagent.h src/posagent_json.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RUNTIME) examples/posagent_demo.c -o $@

$(TEST): $(RUNTIME) tests/test_posagent.c include/posagent.h src/posagent_json.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RUNTIME) tests/test_posagent.c -o $@

test: $(TEST)
	./$(TEST)

run: $(DEMO)
	./$(DEMO)

clean:
	rm -rf $(BUILD_DIR)
