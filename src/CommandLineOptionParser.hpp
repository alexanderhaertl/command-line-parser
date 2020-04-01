#pragma once

#include <string>
#include <map>
#include <functional>

class StringParser
{
public:
  template <typename T>
  static void parseString(const std::string& valueAsString, T& value)
  {
    static_assert(false, "Value type not supported");
  }

  template <> static void parseString(const std::string& valueAsString, int& value)
  {
    value = std::stoi(valueAsString);
  }

  template <> static void parseString(const std::string& valueAsString, unsigned long& value)
  {
    value = std::stoul(valueAsString);
  }

  template <> static void parseString(const std::string& valueAsString, long& value)
  {
    value = std::stol(valueAsString);
  }

  template <> static void parseString(const std::string& valueAsString, unsigned long long& value)
  {
    value = std::stoull(valueAsString);
  }

  template <> static void parseString(const std::string& valueAsString, long long& value)
  {
    value = std::stoll(valueAsString);
  }

  template <> static void parseString(const std::string& valueAsString, float& value)
  {
    value = std::stof(valueAsString);
  }

  template <> static void parseString(const std::string& valueAsString, double& value)
  {
    value = std::stod(valueAsString);
  }

  template <> static void parseString(const std::string& valueAsString, std::string& value)
  {
    value = valueAsString;
  }
};

class CommandLineOptionParser
{
public:
  template <typename T>
  void registerOption(T& parameter, const std::string& optionName)
  {
    m_parserFunctions.emplace(optionName, std::bind(StringParser::parseString<T>, std::placeholders::_1, std::ref(parameter)));
  }

  template <>
  void registerOption(bool& flag, const std::string& optionName)
  {
    m_flags.emplace(optionName, &flag);
  }

  bool parseCommandLineArguments(const std::vector<std::string>& args)
  {
    try
    {
      for (auto i = args.begin(); i != args.end(); ++i)
      {
        auto flagFindResult = m_flags.find(*i);
        auto paramFindResult = m_parserFunctions.find(*i);
        if (flagFindResult != m_flags.end())
        {
          *(flagFindResult->second) = !(*(flagFindResult->second));
        }
        else if (paramFindResult != m_parserFunctions.end())
        {
          if (++i != args.end()) // we have at least one parameter left
          {
            // parameter iterator was intentionally advanced, because the actual option is the parameter following the option identifier
            paramFindResult->second(*i);
          }
          else
          {
            // we have an option expecting a parameter, without a parameter following
            return false;
          }
        }
        else
        {
          // unknown option
          return false;
        }
      }
    }
    catch (const std::invalid_argument&)
    {
      return false;
    }

    return true;
  }

private:
  std::map<std::string, std::function<void(const std::string&)> > m_parserFunctions;
  std::map<std::string, bool*> m_flags;
};