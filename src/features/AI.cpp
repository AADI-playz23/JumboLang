// src/features/AI.cpp
// Real LLM API integration for JumboLang.
// Supports: OpenAI, Anthropic (Claude), OpenAI-compatible (Ollama, Groq, etc.)
// Requires: compile with USE_CURL=1 and a valid API key in the environment.
#include "../../include/features/AI.h"
#include "../../include/features/HTTP.h"
#include <iostream>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
// CONSTRUCTOR
// No hardcoded keys. We attempt to load from standard environment variables.
// ─────────────────────────────────────────────────────────────────────────────
AIManager::AIManager(const std::string& model, AIProvider p,
                     const std::string& customBaseUrl)
    : modelName(model), provider(p), baseUrl(customBaseUrl)
{
    // Try provider-specific env vars first, then the generic JUMBO_API_KEY
    if (p == AIProvider::ANTHROPIC) {
        loadApiKeyFromEnv("ANTHROPIC_API_KEY");
    } else {
        loadApiKeyFromEnv("OPENAI_API_KEY");
    }
    if (apiKey.empty()) loadApiKeyFromEnv("JUMBO_API_KEY");

    if (apiKey.empty()) {
        std::cerr << "    ⚠️  [AI] No API key found in environment.\n"
                  << "         Set OPENAI_API_KEY, ANTHROPIC_API_KEY, or JUMBO_API_KEY.\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// KEY MANAGEMENT
// ─────────────────────────────────────────────────────────────────────────────
void AIManager::setApiKey(const std::string& key) { apiKey = key; }

void AIManager::loadApiKeyFromEnv(const std::string& envVarName) {
    const char* val = std::getenv(envVarName.c_str());
    if (val && std::string(val).length() > 0) apiKey = val;
}

void AIManager::setProvider(AIProvider p) { provider = p; }
void AIManager::setBaseUrl(const std::string& url) { baseUrl = url; }

// ─────────────────────────────────────────────────────────────────────────────
// PROVIDER STRING MAPPING
// ─────────────────────────────────────────────────────────────────────────────
AIProvider AIManager::providerFromString(const std::string& name) {
    if (name == "openai")                               return AIProvider::OPENAI;
    if (name == "anthropic" || name == "claude")        return AIProvider::ANTHROPIC;
    if (name == "compatible" || name == "openai-compatible"
        || name == "ollama"  || name == "groq"
        || name == "together"|| name == "lmstudio")     return AIProvider::OPENAI_COMPATIBLE;
    return AIProvider::OPENAI; // safe default
}

// ─────────────────────────────────────────────────────────────────────────────
// PROMPT SANITISER
// Trims whitespace + JSON-escapes special chars to prevent API payload injection
// ─────────────────────────────────────────────────────────────────────────────
std::string AIManager::sanitizePrompt(const std::string& raw) {
    std::string clean = raw;
    // Trim
    clean.erase(0, clean.find_first_not_of(" \n\r\t"));
    if (!clean.empty()) clean.erase(clean.find_last_not_of(" \n\r\t") + 1);

    // JSON-escape
    std::string escaped;
    escaped.reserve(clean.size());
    for (char c : clean) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            case '\t': escaped += "\\t";  break;
            default:   escaped += c;      break;
        }
    }
    return escaped;
}

// ─────────────────────────────────────────────────────────────────────────────
// PAYLOAD BUILDERS
// ─────────────────────────────────────────────────────────────────────────────
std::string AIManager::buildOpenAIPayload(const std::string& escapedPrompt) const {
    return "{"
           "\"model\": \"" + modelName + "\","
           "\"messages\": [{\"role\": \"user\", \"content\": \"" + escapedPrompt + "\"}],"
           "\"max_tokens\": 2048"
           "}";
}

std::string AIManager::buildAnthropicPayload(const std::string& escapedPrompt) const {
    return "{"
           "\"model\": \"" + modelName + "\","
           "\"max_tokens\": 2048,"
           "\"messages\": [{\"role\": \"user\", \"content\": \"" + escapedPrompt + "\"}]"
           "}";
}

// ─────────────────────────────────────────────────────────────────────────────
// RESPONSE TEXT EXTRACTOR
// Digs out the assistant message text from the raw JSON response body.
// Uses a targeted search rather than a full JSON parse for minimal dependencies.
// ─────────────────────────────────────────────────────────────────────────────
std::string AIManager::extractResponseText(const std::string& json,
                                           AIProvider p) const {
    // Helper lambda: extracts the string value after a given JSON key
    auto extractField = [&](const std::string& key) -> std::string {
        std::string needle = "\"" + key + "\"";
        size_t kPos = json.find(needle);
        if (kPos == std::string::npos) return "";
        size_t cPos = json.find(':', kPos + needle.size());
        if (cPos == std::string::npos) return "";
        size_t sPos = json.find('"', cPos + 1);
        if (sPos == std::string::npos) return "";
        sPos++; // skip the opening quote

        std::string result;
        result.reserve(256);
        for (size_t i = sPos; i < json.size(); ++i) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                char esc = json[++i];
                if      (esc == 'n')  result += '\n';
                else if (esc == 't')  result += '\t';
                else if (esc == '"')  result += '"';
                else if (esc == '\\') result += '\\';
                else                  result += esc;
            } else if (json[i] == '"') {
                break;
            } else {
                result += json[i];
            }
        }
        return result;
    };

    if (p == AIProvider::ANTHROPIC) {
        // Anthropic: {"content":[{"type":"text","text":"..."}],...}
        // Locate the first "type":"text" block, then grab its "text" field
        size_t typePos = json.find("\"type\":\"text\"");
        if (typePos == std::string::npos)
            typePos = json.find("\"type\": \"text\"");
        if (typePos != std::string::npos) {
            // The "text" key that follows
            size_t textKey = json.find("\"text\"", typePos);
            if (textKey != std::string::npos) {
                size_t cPos = json.find(':', textKey + 6);
                size_t sPos = json.find('"', cPos + 1);
                if (sPos != std::string::npos) {
                    sPos++;
                    std::string result;
                    for (size_t i = sPos; i < json.size(); ++i) {
                        if (json[i] == '\\' && i + 1 < json.size()) {
                            char esc = json[++i];
                            if      (esc == 'n')  result += '\n';
                            else if (esc == 't')  result += '\t';
                            else if (esc == '"')  result += '"';
                            else if (esc == '\\') result += '\\';
                            else                  result += esc;
                        } else if (json[i] == '"') {
                            break;
                        } else {
                            result += json[i];
                        }
                    }
                    if (!result.empty()) return result;
                }
            }
        }
        return extractField("text"); // fallback
    } else {
        // OpenAI: {"choices":[{"message":{"role":"assistant","content":"..."}}]}
        // The "content" key appears after "role":"assistant"
        size_t rolePos = json.find("\"role\":\"assistant\"");
        if (rolePos == std::string::npos)
            rolePos = json.find("\"role\": \"assistant\"");

        size_t searchFrom = (rolePos != std::string::npos) ? rolePos : 0;
        size_t cKey = json.find("\"content\"", searchFrom);
        if (cKey != std::string::npos) {
            size_t cPos = json.find(':', cKey + 9);
            size_t sPos = json.find('"', cPos + 1);
            if (sPos != std::string::npos) {
                sPos++;
                std::string result;
                for (size_t i = sPos; i < json.size(); ++i) {
                    if (json[i] == '\\' && i + 1 < json.size()) {
                        char esc = json[++i];
                        if      (esc == 'n')  result += '\n';
                        else if (esc == 't')  result += '\t';
                        else if (esc == '"')  result += '"';
                        else if (esc == '\\') result += '\\';
                        else                  result += esc;
                    } else if (json[i] == '"') {
                        break;
                    } else {
                        result += json[i];
                    }
                }
                if (!result.empty()) return result;
            }
        }
        return extractField("content"); // fallback
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN GENERATE FUNCTION
// Assembles URL + headers + payload, fires the POST, parses the response.
// ─────────────────────────────────────────────────────────────────────────────
std::string AIManager::generateResponse(const std::string& prompt) {
    std::string escapedPrompt = sanitizePrompt(prompt);

    if (apiKey.empty()) {
        return "[AI ERROR] No API key configured. "
               "Set OPENAI_API_KEY, ANTHROPIC_API_KEY, or JUMBO_API_KEY.";
    }

    std::string url, payload;
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";

    switch (provider) {

        case AIProvider::ANTHROPIC:
            std::cout << "    🤖 [AI] Anthropic Claude — model: " << modelName << "\n";
            url     = "https://api.anthropic.com/v1/messages";
            payload = buildAnthropicPayload(escapedPrompt);
            headers["x-api-key"]          = apiKey;
            headers["anthropic-version"]  = "2023-06-01";
            break;

        case AIProvider::OPENAI_COMPATIBLE:
            if (baseUrl.empty()) baseUrl = "http://localhost:11434/v1"; // Ollama default
            std::cout << "    🤖 [AI] OpenAI-Compatible — endpoint: " << baseUrl
                      << " | model: " << modelName << "\n";
            url     = baseUrl + "/chat/completions";
            payload = buildOpenAIPayload(escapedPrompt);
            headers["Authorization"] = "Bearer " + apiKey;
            break;

        case AIProvider::OPENAI:
        default:
            std::cout << "    🤖 [AI] OpenAI — model: " << modelName << "\n";
            url     = "https://api.openai.com/v1/chat/completions";
            payload = buildOpenAIPayload(escapedPrompt);
            headers["Authorization"] = "Bearer " + apiKey;
            break;
    }

    std::cout << "    📡 [AI] Sending " << escapedPrompt.size() << "-byte payload...\n";

    HttpResponse resp = HttpClient::post(url, payload, headers);

    if (!resp.success && resp.statusCode == 0) {
        // libcurl not compiled in or network error
        return "[AI ERROR] " + resp.errorMessage;
    }

    // Check for API-level error object in the JSON body
    if (resp.body.find("\"error\"") != std::string::npos &&
        resp.body.find("\"message\"") != std::string::npos) {
        // Try to extract the error message
        size_t msgPos = resp.body.find("\"message\"");
        if (msgPos != std::string::npos) {
            size_t cPos = resp.body.find(':', msgPos + 9);
            size_t sPos = resp.body.find('"', cPos + 1);
            if (sPos != std::string::npos) {
                sPos++;
                std::string errMsg;
                for (size_t i = sPos; i < resp.body.size() && resp.body[i] != '"'; ++i)
                    errMsg += resp.body[i];
                std::cerr << "    ❌ [AI] API error: " << errMsg << "\n";
                return "[AI ERROR] " + errMsg;
            }
        }
        std::cerr << "    ❌ [AI] API error (HTTP " << resp.statusCode << "): "
                  << resp.body.substr(0, 300) << "\n";
        return "[AI ERROR] HTTP " + std::to_string(resp.statusCode);
    }

    std::string text = extractResponseText(resp.body, provider);
    if (text.empty()) {
        std::cerr << "    ⚠️  [AI] Could not extract text. Raw (first 300 chars):\n"
                  << resp.body.substr(0, 300) << "\n";
        return "[AI ERROR] Unexpected response format";
    }

    return text;
}