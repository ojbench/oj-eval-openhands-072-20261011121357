CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall
TARGET = code
SOURCE = simulator.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean