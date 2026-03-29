APP := bin/lms_api
SOURCES := $(shell find src -name '*.cpp')

CXX ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2 -pthread
POCO_PREFIX ?= /usr
SQLITE_PREFIX ?= /usr

INCLUDES := -I$(POCO_PREFIX)/include -I$(SQLITE_PREFIX)/include
LIBS := -L$(POCO_PREFIX)/lib -L$(SQLITE_PREFIX)/lib -lPocoNet -lPocoUtil -lPocoJSON -lPocoData -lPocoDataSQLite -lPocoFoundation -lsqlite3

.PHONY: all run clean

all: $(APP)

$(APP): $(SOURCES) | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(APP) $(LIBS)

bin:
	mkdir -p bin

run: $(APP)
	PORT=8080 DB_PATH=data/lms.db ./$(APP)

clean:
	rm -rf $(APP) build data/lms.db
