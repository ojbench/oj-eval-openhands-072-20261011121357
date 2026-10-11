CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
TARGET = code
SOURCE = simulator.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean