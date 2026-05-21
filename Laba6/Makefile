CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

COMMON_SRC = src/http_client.cpp src/rabbitmq_client.cpp src/events.cpp

.PHONY: all clean

all: bin/producer bin/consumer

bin:
	mkdir -p bin

bin/producer: bin $(COMMON_SRC) src/producer.cpp
	$(CXX) $(CXXFLAGS) $(COMMON_SRC) src/producer.cpp -o $@

bin/consumer: bin $(COMMON_SRC) src/consumer.cpp
	$(CXX) $(CXXFLAGS) $(COMMON_SRC) src/consumer.cpp -o $@

clean:
	rm -rf bin
