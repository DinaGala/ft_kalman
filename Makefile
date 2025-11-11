# Makefile for Kalman Filter C++ project
# - compiles all .cpp in this directory
# - uses -Wall -Wextra -Werror
# - generates dependency files to avoid unnecessary relinking

CXX := g++
RM := rm -f

CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2 -g
LDFLAGS :=

SRCS := $(wildcard *.cpp)
# place object files and dependency files under this directory
DEPDIR := incs
OBJS := $(patsubst %.cpp,$(DEPDIR)/%.o,$(SRCS))
# dependency files are stored under $(DEPDIR)
DEPS := $(OBJS:.o=.d)

TARGET := kalman

.PHONY: all clean fclean re

all: $(DEPDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(DEPDIR)/%.o: %.cpp | $(DEPDIR)
	# Generate object file and write dependency file into $(DEPDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD -MP -MF $(DEPDIR)/$*.d

# Include auto-generated dependency files (if present). This avoids relinking
# when nothing has changed that requires a link.
-include $(DEPS)

# Ensure dependency directory exists before compiling
$(DEPDIR):
	mkdir -p $(DEPDIR)

clean:
	$(RM) $(OBJS) $(DEPS)

fclean: clean
	$(RM) $(TARGET)

re: fclean all
