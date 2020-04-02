#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <functional>

class StringParser
{
public:
  template <typename T>
  static bool parseString(const std::string& valueAsString, T& value)
  {
    return (bool)(std::stringstream(valueAsString) >> value);
  }

  template <> static bool parseString(const std::string& valueAsString, std::string& value)
  {
    value = valueAsString;
    return true;
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
          if (!paramFindResult->second(*i))
          {
            // parsing error
            return false;
          }
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

    return true;
  }

private:
  std::map<std::string, std::function<bool(const std::string&)> > m_parserFunctions;
  std::map<std::string, bool*> m_flags;
};