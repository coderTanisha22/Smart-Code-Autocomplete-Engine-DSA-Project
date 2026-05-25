CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -pthread
INCLUDES = -Iinclude
LIBS = -lncurses -pthread

ENGINE = src/tst.cpp \
	src/phrase_store.cpp \
	src/freq_store.cpp \
	src/ranker.cpp \
	src/graph.cpp \
	src/minheap.cpp \
	src/lru.cpp \
	src/stack.cpp \
	src/kmp.cpp

SRC = basic_editor.cpp $(ENGINE)

TARGET = basic_editor
TESTBIN = build/tests

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(INCLUDES) $(LIBS) -o $(TARGET)

# Headless REPL for exercising the engine without ncurses.
cli: src/main.cpp $(ENGINE)
	$(CXX) $(CXXFLAGS) src/main.cpp $(ENGINE) $(INCLUDES) -pthread -o smart_autocomplete

# Each test links only the modules it exercises.
test: $(TESTBIN)/tst_test $(TESTBIN)/heap_test $(TESTBIN)/lru_test $(TESTBIN)/regression_test
	@echo "--- tst ---"        && $(TESTBIN)/tst_test
	@echo "--- heap ---"       && $(TESTBIN)/heap_test
	@echo "--- lru ---"        && $(TESTBIN)/lru_test
	@echo "--- regression ---" && $(TESTBIN)/regression_test

$(TESTBIN):
	mkdir -p $(TESTBIN)

$(TESTBIN)/tst_test: tests/tst_test.cpp src/tst.cpp | $(TESTBIN)
	$(CXX) $(CXXFLAGS) $^ $(INCLUDES) -o $@

$(TESTBIN)/heap_test: tests/heap_test.cpp src/minheap.cpp | $(TESTBIN)
	$(CXX) $(CXXFLAGS) $^ $(INCLUDES) -o $@

$(TESTBIN)/lru_test: tests/lru_test.cpp src/lru.cpp | $(TESTBIN)
	$(CXX) $(CXXFLAGS) $^ $(INCLUDES) -o $@

$(TESTBIN)/regression_test: tests/regression_test.cpp src/tst.cpp src/freq_store.cpp src/stack.cpp src/phrase_store.cpp | $(TESTBIN)
	$(CXX) $(CXXFLAGS) $^ $(INCLUDES) -o $@

clean:
	rm -f $(TARGET) smart_autocomplete
	rm -rf $(TESTBIN)

.PHONY: all cli test clean
