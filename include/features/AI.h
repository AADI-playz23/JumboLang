// include/features/AI.h
// Supports OpenAI, Anthropic (Claude), and any OpenAI-compatible endpoint
// (e.g., Ollama, Groq, Together AI, LM Studio).
// API keys are NEVER hardcoded — they are always read from environment variables.
#ifndef JUMBOLANG_AI_H
#define JUMBOLANG_AI_H

#include <string>
#include <vector>

// ── Supported LLM providers ───────────────────────────────────────────────────
enum class AIProvider {
    OPENAI,             // https://api.openai.com   — gpt-4o, gpt-4-turbo, etc.
    ANTHROPIC,          // https://api.anthropic.com — claude-3-5-sonnet, etc.
    OPENAI_COMPATIBLE,  // Any OpenAI-style /chat/completions endpoint
    NONE                // Simulation-only (no curl / no key configured)
};

class AIManager {
private:
    std::string apiKey;
    std::string modelName;
    AIProvider  provider;
    std::string baseUrl;   // Used for OPENAI_COMPATIBLE providers

    // ── Payload builders (return JSON strings ready to POST) ──────────────────
    std::string buildOpenAIPayload(const std::string& escapedPrompt)    const;
    std::string buildAnthropicPayload(const std::string& escapedPrompt) const;

    // ── Response parser — extracts the assistant text from the raw JSON reply ──
    std::string extractResponseText(const std::string& rawJson,
                                    AIProvider p) const;

public:
    // model  — e.g. "gpt-4o", "claude-3-5-sonnet-20241022"
    // p      — which provider to use
    // customBaseUrl — required for OPENAI_COMPATIBLE (e.g. "http://localhost:11434/v1")
    AIManager(const std::string& model,
              AIProvider p = AIProvider::OPENAI,
              const std::string& customBaseUrl = "");

    // ── Key management (reads from process environment, never hardcoded) ───────
    void setApiKey(const std::string& key);
    void loadApiKeyFromEnv(const std::string& envVarName);

    void setProvider(AIProvider p);
    void setBaseUrl(const std::string& url);

    // ── Core function: trims prompt, builds payload, POSTs, returns text ──────
    std::string generateResponse(const std::string& prompt);

    // Trims whitespace AND JSON-escapes special characters to prevent injection
    std::string sanitizePrompt(const std::string& raw);

    // Convert a provider name string to the enum (used by the interpreter)
    static AIProvider providerFromString(const std::string& name);
};

#endif // JUMBOLANG_AI_H