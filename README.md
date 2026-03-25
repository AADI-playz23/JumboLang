# 🐘 JumboLang (.jl)
**The Sovereign, AI-Native Systems Language.**

JumboLang is a high-performance backend language built in C++ designed to bridge the gap between the simplicity of Python and the raw power of C. It features a custom compiler pipeline (Lexer, Parser, AST) and a modular Virtual Machine.

## 🚀 Core Features
- **Native AI Routing**: Built-in `{llm}` tags for zero-boilerplate AI integration (Gemini-ready).
- **High-Speed Networking**: Direct Linux Socket integration for `{https}` and `{wss}` servers.
- **Persistence Layer**: Native `{file}` system and `{db}` Key-Value storage.
- **Data Native**: Built-in `{json}` parsing that handles raw web data safely.
- **Memory Safe**: Utilizes C++ Smart Pointers for automatic resource management.

## 🛠️ Getting Started

### Prerequisites
- G++ Compiler (C++17 or higher)
- Make

### Installation
```bash
git clone [https://github.com/AADI-playz23/JumboLang.git](https://github.com/AADI-playz23/JumboLang.git)
cd JumboLang
make build


###Run a Script

./jumbol tests/app.jl


##📝 Example Syntax
{main}
    {https port="8080"}
        {json action="parse"}
            { "status": "active", "dev": "AADI" }
        {-json}
        
        {llm model="gemini"}
            Generate a welcome message for the user.
        {-llm}
    {-https}
{-main}















