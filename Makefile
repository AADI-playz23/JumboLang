build:
	g++ src/main.cpp src/Lexer.cpp src/Parser.cpp src/Interpreter.cpp src/features/Network.cpp src/features/AI.cpp src/features/FileSystem.cpp src/features/Database.cpp src/features/JSON.cpp -o jumbol -Iinclude -O3

run: build
	./jumbol tests/app.jl