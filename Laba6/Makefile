CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
LIBS ?= -lPocoNet -lPocoJSON -lPocoFoundation -lrabbitmq

COMMON_SRC = src/rabbitmq_client.cpp src/events.cpp

.PHONY: all clean

all: bin/producer bin/consumer

bin:
	mkdir -p bin

bin/producer: bin $(COMMON_SRC) src/producer.cpp
	$(CXX) $(CXXFLAGS) $(COMMON_SRC) src/producer.cpp $(LIBS) -o $@

bin/consumer: bin $(COMMON_SRC) src/consumer.cpp
	$(CXX) $(CXXFLAGS) $(COMMON_SRC) src/consumer.cpp $(LIBS) -o $@

clean:
	rm -rf bin
