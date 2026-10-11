CXX = g++
CXXFLAGS = -O2 -Wall
TARGET = code
SOURCE = simulator_simple.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean