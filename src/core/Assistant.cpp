#include "Assistant.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

Assistant::Assistant()
    : running(false)
    , conversationHistory(json::array())
{
    const char *key = std::getenv("GEMINI_API_KEY");

    if (key)
        apiKey = key;
}

void Assistant::start()
{
    running = true;

    displayWelcome();

    if (apiKey.empty())
    {
        std::cerr << "JARVIS > GEMINI_API_KEY is not configured.\n";
        return;
    }

    std::string input;

    while (running)
    {
        std::cout << "\nYOU > ";

        if (!std::getline(std::cin, input))
            break;

        processCommand(input);
    }

    std::cout << "\nJARVIS > Shutting down. Goodbye.\n";
}

void Assistant::displayWelcome()
{
    std::cout << R"(
      ██╗ █████╗ ██████╗ ██╗   ██╗██╗███████╗
      ██║██╔══██╗██╔══██╗██║   ██║██║██╔════╝
      ██║███████║██████╔╝██║   ██║██║███████╗
 ██   ██║██╔══██║██╔══██║╚██╗ ██╔╝██║╚════██║
 ╚█████╔╝██║  ██║██║  ██║ ╚████╔╝ ██║███████║
  ╚════╝ ╚═╝  ╚═╝╚═╝  ╚═╝  ╚═══╝  ╚═╝╚══════╝
    )" << '\n';

    std::cout << "JARVIS SYSTEM INITIALIZED.\n";
    std::cout << "Gemini connection ready.\n";
    std::cout << "Type 'help' to see available commands.\n";
}

void Assistant::processCommand(const std::string &command)
{
    std::string cmd = command;

    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (cmd.empty())
        return;

    if (cmd == "exit" || cmd == "quit")
    {
        running = false;
    }
    else if (cmd == "help")
    {
        displayHelp();
    }
    else if (cmd == "time")
    {
        std::cout << "JARVIS > The current time is " << getCurrentTime()
                  << '\n';
    }
    else if (cmd == "status")
    {
        std::cout << "JARVIS > All core systems operational.\n";
    }
    else if (cmd == "clear")
    {
        conversationHistory = json::array();
        std::cout << "JARVIS > Conversation history cleared.\n";
    }
    else
    {
        generateAIResponse(command);
    }
}

void Assistant::displayHelp()
{
    std::cout << "\nAvailable commands:\n";
    std::cout << "  help   - Display commands\n";
    std::cout << "  time   - Get current time\n";
    std::cout << "  status - Check system status\n";
    std::cout << "  clear  - Clear conversation history\n";
    std::cout << "  exit   - Shut down JARVIS\n";

    std::cout << "\nYou can also ask Gemini questions.\n";
}

std::string Assistant::getSystemInstruction()
{
    return R"(
You are JARVIS, a personal AI assistant running on a Linux Debian system.

You are professional, intelligent, concise and helpful.

You have access to tools that provide information about the local computer.

Rules:
- Use tools when they are useful to answer a request.
- Never claim to have performed an action unless a tool actually performed it.
- Explain what information you retrieved from tools.
- Do not invent system information.
- Do not attempt to execute shell commands.
- Ask for clarification when a request is ambiguous.
- Be honest about your limitations.
)";
}

void Assistant::generateAIResponse(const std::string &prompt)
{
    const std::string url =
        "https://generativelanguage.googleapis.com/v1beta/models/"
        + std::string(MODEL) + ":generateContent";

    conversationHistory.push_back(
        { { "role", "user" },
          { "parts", json::array({ { { "text", prompt } } }) } });

    // Keep the most recent messages.
    while (conversationHistory.size() > MAX_HISTORY)
        conversationHistory.erase(conversationHistory.begin());

    for (int round = 0; round < MAX_TOOL_ROUNDS; ++round)
    {
        json request = {
            { "systemInstruction",
              { { "parts",
                  json::array({ { { "text", getSystemInstruction() } } }) } } },
            { "contents", conversationHistory },
            { "tools",
              json::array({ { { "functionDeclarations",
                                toolManager.getDeclarations() } } }) },
            { "generationConfig",
              { { "temperature", 0.7 }, { "maxOutputTokens", 2048 } } }
        };

        HttpResponse response =
            cloudClient.postJson(url, apiKey, request.dump());

        if (response.statusCode < 200 || response.statusCode >= 300)
        {
            std::cerr << "JARVIS > Gemini API error (HTTP "
                      << response.statusCode << ")\n";

            try
            {
                json error = json::parse(response.body);

                if (error.contains("error")
                    && error["error"].contains("message"))
                {
                    std::cerr << error["error"]["message"] << '\n';
                }
                else
                {
                    std::cerr << response.body << '\n';
                }
            }
            catch (...)
            {
                std::cerr << response.body << '\n';
            }

            return;
        }

        json result;

        try
        {
            result = json::parse(response.body);
        }
        catch (const json::exception &e)
        {
            std::cerr << "JARVIS > Invalid Gemini response: " << e.what()
                      << '\n';
            return;
        }

        if (!result.contains("candidates") || result["candidates"].empty()
            || !result["candidates"][0].contains("content"))
        {
            std::cerr << "JARVIS > Gemini returned no usable response.\n";
            return;
        }

        const json modelContent = result["candidates"][0]["content"];

        if (!modelContent.contains("parts")
            || !modelContent["parts"].is_array())
        {
            std::cerr << "JARVIS > Gemini returned invalid content.\n";
            return;
        }

        bool hasFunctionCall = false;

        for (const auto &part : modelContent["parts"])
        {
            if (part.contains("functionCall"))
            {
                hasFunctionCall = true;
                break;
            }
        }

        // Normal text response: display it and save it.
        if (!hasFunctionCall)
        {
            conversationHistory.push_back(modelContent);

            std::string answer;

            for (const auto &part : modelContent["parts"])
            {
                if (part.contains("text") && part["text"].is_string())
                {
                    answer += part["text"].get<std::string>();
                }
            }

            if (answer.empty())
                answer = "I received an empty response.";

            std::cout << "\nJARVIS > " << answer << '\n';
            return;
        }

        // Save Gemini's function-call message in the history.
        conversationHistory.push_back(modelContent);

        // Execute all requested function calls.
        json functionResponses = json::array();

        for (const auto &part : modelContent["parts"])
        {
            if (!part.contains("functionCall"))
                continue;

            const json call = part["functionCall"];

            const std::string name = call.value("name", "");

            const json arguments = call.value("args", json::object());

            std::cout << "\nJARVIS [TOOL] > " << name << '\n';

            json toolResult = toolManager.execute(name, arguments);

            std::cout << "JARVIS [TOOL RESULT] > " << toolResult.dump() << '\n';

            functionResponses.push_back(
                { { "functionResponse",
                    { { "name", name }, { "response", toolResult } } } });
        }

        // Send tool results back to Gemini.
        conversationHistory.push_back(
            { { "role", "user" }, { "parts", functionResponses } });
    }

    std::cout << "\nJARVIS > Maximum tool-call limit reached. "
              << "Please try a simpler request.\n";
}

std::string Assistant::getCurrentTime()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%H:%M:%S");

    return stream.str();
}
