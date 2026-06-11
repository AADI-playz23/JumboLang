# ─── JUMBOLANG MAKEFILE ───────────────────────────────────────────────────────
# Usage:
#   make              — build without libcurl (AI/fetch use stub messages)
#   make USE_CURL=1   — build with real HTTP support (requires libcurl)
#   make run          — build + run tests/app.jl
#   make clean        — remove compiled binary
#
# Install libcurl:
#   Linux:   sudo apt-get install libcurl4-openssl-dev
#   macOS:   brew install curl
#   Windows: vcpkg install curl  OR  mingw-w64 ships curl via pacman
# ─────────────────────────────────────────────────────────────────────────────

CXX      = g++
TARGET   = jumbol
CXXFLAGS = -Iinclude -O2 -std=c++17 -Wall -Wextra -Wpedantic
LDFLAGS  =

# ── Source files ──────────────────────────────────────────────────────────────
SRCS = src/main.cpp          \
       src/Lexer.cpp          \
       src/Parser.cpp         \
       src/Interpreter.cpp    \
       src/features/Network.cpp  \
       src/features/AI.cpp       \
       src/features/HTTP.cpp     \
       src/features/FileSystem.cpp \
       src/features/Database.cpp   \
       src/features/JSON.cpp

# ── Optional: libcurl for real AI / fetch calls ────────────────────────────
ifdef USE_CURL
CXXFLAGS += -DHAVE_CURL

ifeq ($(OS),Windows_NT)
    # Windows (MinGW): adjust path if curl is installed elsewhere
    CURL_PREFIX ?= C:/msys64/mingw64
    CXXFLAGS += -I$(CURL_PREFIX)/include
    LDFLAGS  += -L$(CURL_PREFIX)/lib -lcurl -lws2_32
else
    # Linux / macOS
    LDFLAGS  += -lcurl
endif
endif

# ── Thread support ────────────────────────────────────────────────────────────
ifeq ($(OS),Windows_NT)
    # MinGW links pthreads automatically for std::thread
else
    LDFLAGS += -lpthread
endif

# ── Targets ───────────────────────────────────────────────────────────────────
.PHONY: all build run clean help

all: build

## build: compile JumboLang
build:
	@echo "🐘 Compiling JumboLang..."
ifdef USE_CURL
	@echo "   🔌 libcurl enabled (real AI + fetch calls active)"
else
	@echo "   ⚠️  libcurl disabled — use 'make USE_CURL=1' for network features"
endif
	$(CXX) $(SRCS) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS)
	@echo "✅ Build complete: ./$(TARGET)"

## run: build and execute tests/app.jl
run: build
	@echo "🚀 Executing tests/app.jl..."
	./$(TARGET) tests/app.jl

## clean: remove the compiled binary
clean:
	@echo "🧹 Cleaning up..."
	rm -f $(TARGET)

## help: show this message
help:
	@echo "JumboLang Build System"
	@echo ""
	@echo "  make              Build without curl (stub mode)"
	@echo "  make USE_CURL=1   Build with real HTTP/AI support"
	@echo "  make run          Build + run tests/app.jl"
	@echo "  make clean        Remove compiled binary"