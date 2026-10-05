CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
LDFLAGS  ?= -pthread

DEPS := src/arena.h src/spsc.h

all: arena_test spsc_test

arena_test: src/arena.cpp src/arena_test.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -o $@ src/arena.cpp src/arena_test.cpp $(LDFLAGS)

spsc_test: src/arena.cpp src/spsc_test.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -o $@ src/arena.cpp src/spsc_test.cpp $(LDFLAGS)

check: all
	./arena_test
	@echo
	./spsc_test

# memory errors
asan: CXXFLAGS := -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -pthread
asan: LDFLAGS  := -fsanitize=address,undefined -pthread
asan: clean all

# data races. setarch -R turns off ASLR for the run, otherwise TSan dies with
# "unexpected memory mapping" on recent kernels.
tsan: CXXFLAGS := -std=c++17 -O1 -g -Wall -Wextra -fsanitize=thread -pthread
tsan: LDFLAGS  := -fsanitize=thread -pthread
tsan: clean all

tsan-check: tsan
	setarch $$(uname -m) -R ./spsc_test

clean:
	rm -f arena_test spsc_test

.PHONY: all check asan tsan tsan-check clean
