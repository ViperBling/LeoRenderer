#pragma once

#include <iostream>
#include <unordered_map>
#include <string>
#include <utility>
#include <cassert>

struct CmdLineOption
{
    bool mbHasValue = false;
    bool mbSet = false;
    std::string mHelp;
    std::string mValue;
    std::vector<std::string> mCmds;
};

class CmdLineParser
{
public:
    void Add(std::string name, std::vector<std::string> commands, bool hasValue, std::string help)
    {
        mOptions[name].mCmds = std::move(commands);
        mOptions[name].mHelp = std::move(help);
        mOptions[name].mbSet = false;
        mOptions[name].mbHasValue = hasValue;
        mOptions[name].mValue = "";
    }

    void PrintHelp()
    {
        std::cout << "Available command line options:\n";
        for (auto& option : mOptions)
        {
            std::cout << " ";
            for (size_t i = 0; i < option.second.mCmds.size(); i++)
            {
                std::cout << option.second.mCmds[i];
                if (i < option.second.mCmds.size() - 1)
                {
                    std::cout << ", ";
                }
            }
            std::cout << ": " << option.second.mHelp << "\n";
        }
        std::cout << "Press any key to close...";
    }

    void Parse(std::vector<const char*> args)
    {
        bool printHelp = false;
        // Known arguments
        for (auto& option : mOptions)
        {
            for (auto& command : option.second.mCmds)
            {
                for (size_t i = 0; i < args.size(); i++)
                {
                    if (strcmp(args[i], command.c_str()) == 0)
                    {
                        option.second.mbSet = true;
                        // Get value
                        if (option.second.mbHasValue)
                        {
                            if (args.size() > i + 1)
                            {
                                option.second.mValue = args[i + 1];
                            }
                            if (option.second.mValue.empty())
                            {
                                printHelp = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
        // Print help for unknown arguments or missing argument values
        if (printHelp) mOptions["help"].mbSet = true;
    }

    void Parse(int argc, char* argv[])
    {
        std::vector<const char*> args;
        for (int i = 0; i < argc; i++)
        {
            args.push_back(argv[i]);
        };
        Parse(args);
    }

    bool IsSet(std::string name)
    {
        return ((mOptions.find(name) != mOptions.end()) && mOptions[name].mbSet);
    }

    std::string GetValueAsString(std::string name, std::string defaultValue)
    {
        assert(mOptions.find(name) != mOptions.end());
        std::string value = mOptions[name].mValue;
        return (!value.empty()) ? value : defaultValue;
    }

    int32_t GetValueAsInt(std::string name, int32_t defaultValue)
    {
        assert(mOptions.find(name) != mOptions.end());
        std::string value = mOptions[name].mValue;
        if (value != "")
        {
            char* numConvPtr;
            int32_t intVal = strtol(value.c_str(), &numConvPtr, 10);
            return (intVal > 0) ? intVal : defaultValue;
        }
        else
        {
            return defaultValue;
        }
        return int32_t();
    }

public:
    std::unordered_map<std::string, CmdLineOption> mOptions;
};