# Makefile for Kalman Filter C++ project
# - compiles all .cpp in this directory
# - uses -Wall -Wextra -Werror
# - generates dependency files to avoid unnecessary relinking

CXX := g++
RM := rm -f

CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2 -g -MMD -MP
LDFLAGS :=

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

TARGET := kalman

.PHONY: all clean fclean re

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include auto-generated dependency files (if present). This avoids relinking
# when nothing has changed that requires a link.
-include $(DEPS)

clean:
	$(RM) $(OBJS) $(DEPS)

fclean: clean
	$(RM) $(TARGET)

re: fclean all
