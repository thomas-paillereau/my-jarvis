#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "ai/CloudClient.h"
#include "tools/ToolManager.h"

class Assistant
{
public:
    Assistant();

    void start();

private:
    bool running;

    CloudClient cloudClient;
    ToolManager toolManager;

    std::string apiKey;

    // Stores complete Gemini content objects, including function calls.
    nlohmann::json conversationHistory;

    static constexpr const char *MODEL = "gemini-3.5-flash";
    static constexpr std::size_t MAX_HISTORY = 20;
    static constexpr int MAX_TOOL_ROUNDS = 5;

    void processCommand(const std::string &command);
    void displayWelcome();
    void displayHelp();

    void generateAIResponse(const std::string &prompt);

    std::string getCurrentTime();
    static std::string getSystemInstruction();
};
