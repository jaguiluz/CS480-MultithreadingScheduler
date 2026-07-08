# CXX Make variable for compiler
CXX=g++
# -std=c++11  C/C++ variant to use, e.g. C++ 2011
# -Wall       show the necessary warning files
# -g3         include information for symbolic debugger e.g. gdb
CXXFLAGS=-std=c++11 -Wall -g3 -c -lpthread -lrt

# object files
OBJS = main.o log.o producer.o consumer.o

# Program name
PROGRAM = concurrent_scheduler

# Rules format:
# target : dependency1 dependency2 ... dependencyN
#     Command to make target, uses default rules if not specified

# First target is the one executed if you just type make
# make target specifies a specific target
# $^ is an example of a special variable.  It substitutes all dependencies
$(PROGRAM) : $(OBJS)
	$(CXX) -o $(PROGRAM) $^

main.o : main.cpp log.h producer.h consumer.h
	$(CXX) $(CXXFLAGS) main.cpp

log.o : log.h task_types.h log.cpp main.cpp
	$(CXX) $(CXXFLAGS) log.cpp

producer.o : producer.h producer.cpp log.h main.cpp
	$(CXX) $(CXXFLAGS) producer.cpp

consumer.o : consumer.h consumer.cpp producer.h main.cpp
	$(CXX) $(CXXFLAGS) consumer.cpp

clean :
	rm -f *.o $(PROGRAM)
