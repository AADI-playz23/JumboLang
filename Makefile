# --- JUMBOLANG CODESPACE MAKEFILE ---

CXX = g++
CXXFLAGS = -Iinclude -O3 -std=c++17 -Wall
TARGET = jumbol

# Explicitly defining your source files
SRCS = src/main.cpp \
       src/Lexer.cpp \
       src/Parser.cpp \
       src/Interpreter.cpp \
       src/features/Network.cpp \
       src/features/AI.cpp \
       src/features/FileSystem.cpp \
       src/features/Database.cpp \
       src/features/JSON.cpp

# Default command
all: build

# 1. The Build Command
build:
	@echo "🐘 Compiling JumboLang..."
	$(CXX) $(SRCS) -o $(TARGET) $(CXXFLAGS)
	@echo "✅ Build Complete!"

# 2. The Run Command (Automatically builds first)
run: build
	@echo "🚀 Executing tests/app.jl..."
	./$(TARGET) tests/app.jl

# 3. Clean Workspace
clean:
	@echo "🧹 Cleaning up..."
	rm -f $(TARGET)