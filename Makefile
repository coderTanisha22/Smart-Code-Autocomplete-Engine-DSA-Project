CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
INCLUDES = -Iinclude
LIBS = -lncurses

SRC = basic_editor.cpp \
    src/tst.cpp \
	src/phrase_store.cpp \
	src/freq_store.cpp \
	src/ranker.cpp \
	src/graph.cpp \
	src/minheap.cpp \
	src/lru.cpp \
	src/stack.cpp \
	src/kmp.cpp

TARGET = basic_editor

all: $(TARGET)

$(TARGET):
	$(CXX) $(CXXFLAGS) $(SRC) $(INCLUDES) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
