APP := bin/lms_api
SOURCES := $(shell find src -name '*.cpp')

CXX ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2 -pthread
POCO_PREFIX ?= /usr
PQXX_CFLAGS := $(shell pkg-config --cflags libpqxx)
PQXX_LIBS := $(shell pkg-config --libs libpqxx)

INCLUDES := -I$(POCO_PREFIX)/include $(PQXX_CFLAGS)
LIBS := -L$(POCO_PREFIX)/lib -lPocoNet -lPocoUtil -lPocoJSON -lPocoFoundation $(PQXX_LIBS)

.PHONY: all run clean

all: $(APP)

$(APP): $(SOURCES) | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(APP) $(LIBS)

bin:
	mkdir -p bin

run: $(APP)
	PORT=8080 DATABASE_URL=postgresql://lms:lms@127.0.0.1:5432/lms_db ./$(APP)

clean:
	rm -rf $(APP) build
