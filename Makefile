CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread

SRCS = main.cpp playlist.cpp audio_engine.cpp miniaudio.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = reproductor

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
