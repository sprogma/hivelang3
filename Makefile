CXX := clang++

SHELL := /bin/bash

CXXFLAGS := -std=gnu++2c -O3 -g -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE

SOURCES := $(shell find . -name "*.cpp")
HEADERS := $(shell find . -name "*.hpp")
OBJECTS := $(patsubst %.cpp, obj/%.o, $(SOURCES))
TARGET  := a.out

YELLOW := \033[0;33m
RED    := \033[0;31m
GREEN  := \033[0;32m
NC     := \033[0m 

.PHONY: all clean message

all: message $(TARGET)

message:
	@echo -e "$(RED)Waring: Using precompiled grammar files, to update them use guide in 'grammar' directory$(NC)"

$(TARGET): $(OBJECTS)
	@echo -e "$(YELLOW)Linking...$(NC)"
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET)

obj/%.o: %.cpp $(HEADERS)
	@mkdir -p obj
	@echo -e "$(YELLOW)Building $<...$(NC)"
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -rf obj $(TARGET)

