CC=g++
CFLAGS=-c -Wall -g -fPIC
INCLUDE=./
LINKFLAGS=-L./
LIBS=-lgdbapi -lreadline -lopcodes -lbfd -liberty -ldecnumber -pthread -Wl,--no-as-needed -ldl \
	-lncurses -liberty -lgnu
SOURCES=$(wildcard *.cpp)
HEADERS=$(wildcard $(INCLUDE)*.h)
OBJECTS=$(patsubst %.cpp,%.o,$(SOURCES))
TARGET=thread_test

all: $(TARGET)

$(TARGET): $(OBJECTS) libgdbapi.a
	$(CC) -o $(TARGET) $(OBJECTS) $(LINKFLAGS) $(LIBS)
thread_test.o: thread_test.cpp $(HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE) -o $@ thread_test.cpp

cmdline_parser.o: cmdline_parser.cpp $(HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE) -o $@ cmdline_parser.cpp

experiment.o: experiment.cpp $(HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE) -o $@ experiment.cpp

experiment_test.o: experiment_test.cpp $(HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE) -o $@ experiment_test.cpp

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: install
install:
	cp $(TARGET) bin/
