# Mixer Fader Driver - Makefile
# Compiler: MinGW-w64 g++

CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2
INCLUDES := -I include -I ../firmware/mixer-fader/include
LDFLAGS  := -lwinusb -lsetupapi -lole32 -luuid -lshell32 -lgdi32

SRCDIR   := src
BUILDDIR := build
TARGET   := $(BUILDDIR)/mixer-fader-driver.exe

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

run: $(TARGET)
	@echo "[INFO] Running: $(TARGET)"
	@$(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "[OK] Built: $@"

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	del /Q $(BUILDDIR)\*.o $(BUILDDIR)\*.exe 2>nul || true
