CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O3

SRC_CPP = bf.cpp
SRC_C   = bf.c

TARGET_CPP = bfcpp
TARGET_C   = bfc

.PHONY: all clean

default: all

all: $(TARGET_CPP) $(TARGET_C)

$(TARGET_CPP): $(SRC_CPP)
	$(CXX) $(CXXFLAGS) -o $@ $<
	@echo "C++ version built successfully -> $(TARGET_CPP)"

$(TARGET_C): $(SRC_C)
	$(CC) $(CFLAGS) -o $@ $<
	@echo "C version built successfully -> $(TARGET_C)"

clean:
	@echo "Cleaning up generated files..."
	rm -f $(TARGET_CPP) $(TARGET_C)
