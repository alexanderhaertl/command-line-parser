#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <functional>
#include <algorithm>

namespace StringParsing
{
  template <typename T> bool parseString(const std::string& valueAsString, T& value)
  {
    return (bool)(std::stringstream(valueAsString) >> value);
  }

  template <> bool parseString(const std::string& valueAsString, std::string& value)
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
    m_parserFunctions.emplace(optionName, std::bind(StringParsing::parseString<T>, std::placeholders::_1, std::ref(parameter)));
  }

  void registerSwitch(bool& parameter, const std::string& parameterName)
  {
    m_flags.emplace(parameterName, &parameter);
  }

  template <typename T>
  void registerUnnamedParameter(T& parameter, const std::string& parameterName, bool mandatory = true)
  {
    m_unnamedParameters.push_back({
      std::bind(StringParsing::parseString<T>, std::placeholders::_1, std::ref(parameter)),
      parameterName, 
      mandatory });
  }

  bool parseCommandLineArguments(int argc, char** argv)
  {
    return parseCommandLineArguments(std::vector<std::string>(argv, argv + argc));
  }

  bool parseCommandLineArguments(const std::vector<std::string>& args) const
  {
    std::vector<const UnnamedParameter*> mandatoryParams, optionalParams;
    for (const auto& i : m_unnamedParameters)
    {
      (i.mandatory ? mandatoryParams : optionalParams).push_back(&i);
    }
    auto mandatoryParamIterator = mandatoryParams.begin();
    auto optionalParamIterator = optionalParams.begin();

    for (auto i = ++args.begin(); i != args.end(); ++i)
    {
      if (i->substr(0, 1) == "-")
      {
        // named option
        const auto optionIdentifier = i->substr(1);
        auto flagFindResult = m_flags.find(optionIdentifier);
        auto paramFindResult = m_parserFunctions.find(optionIdentifier);
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
      else
      {
        // unnamed option
        if (mandatoryParamIterator != mandatoryParams.end())
        {
          // we have mandatory parameter to match
          if (!(**mandatoryParamIterator++).parserFunction(*i)) return false;
        }
        else if (optionalParamIterator != optionalParams.end())
        {
          // we have an optional parameter to match
          if (!(**optionalParamIterator++).parserFunction(*i)) return false;
        }
        else
        {
          // we have an unnamed parameter without registered counterpart
          return false;
        }
      }
    }

    // check that all mandatory parameters are matched
    if (mandatoryParamIterator != mandatoryParams.end())
    {
      // there are remaining mandatory parameters without matching command line argument
      return false;
    }

    return true;
  }

private:
  struct UnnamedParameter
  {
    std::function<bool(const std::string&)> parserFunction;
    std::string name;
    bool mandatory;
  };

  std::vector<UnnamedParameter> m_unnamedParameters;
  std::map<std::string, std::function<bool(const std::string&)> > m_parserFunctions;
  std::map<std::string, bool*> m_flags;
};