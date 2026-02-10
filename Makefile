CXX := g++
CXXFLAGS := -O2 -std=c++20 -march=native -Wall -Wextra -pedantic
TARGET := latency
SRC := src/main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
