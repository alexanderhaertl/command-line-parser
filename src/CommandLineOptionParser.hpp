#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <functional>
#include <algorithm>
#include <filesystem>

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
  void registerOption(T& parameter, const std::string& optionName, const std::string& parameterName, const std::string& description)
  {
    m_parserFunctions.emplace(optionName, Parameter({
      std::bind(StringParsing::parseString<T>, std::placeholders::_1, std::ref(parameter)),
      parameterName,
      description }));
  }

  void registerSwitch(bool& parameter, const std::string& parameterName, const std::string& description)
  {
    m_flags.emplace(parameterName, Switch({ &parameter, description }));
  }

  template <typename T>
  void registerUnnamedParameter(T& parameter, const std::string& parameterName, const std::string& description, bool mandatory = true)
  {
    (mandatory ? m_mandatoryParameters : m_optionalParameters).push_back({
      std::bind(StringParsing::parseString<T>, std::placeholders::_1, std::ref(parameter)),
      parameterName,
      description});
  }

  bool parseCommandLineArguments(int argc, char** argv)
  {
    return parseCommandLineArguments(std::vector<std::string>(argv, argv + argc));
  }

  bool parseCommandLineArguments(const std::vector<std::string>& args) const
  {
    auto mandatoryParamIterator = m_mandatoryParameters.begin();
    auto optionalParamIterator = m_optionalParameters.begin();

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
          auto& flag = *flagFindResult->second.flagPointer;
          flag = !flag;
        }
        else if (paramFindResult != m_parserFunctions.end())
        {
          if (++i != args.end()) // we have at least one parameter left
          {
            // parameter iterator was intentionally advanced, because the actual option is the parameter following the option identifier
            if (!paramFindResult->second.parserFunction(*i))
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
        if (mandatoryParamIterator != m_mandatoryParameters.end())
        {
          // we have mandatory parameter to match
          if (!mandatoryParamIterator++->parserFunction(*i)) return false;
        }
        else if (optionalParamIterator != m_optionalParameters.end())
        {
          // we have an optional parameter to match
          if (!optionalParamIterator++->parserFunction(*i)) return false;
        }
        else
        {
          // we have an unnamed parameter without registered counterpart
          return false;
        }
      }
    }

    // check that all mandatory parameters are matched
    if (mandatoryParamIterator != m_mandatoryParameters.end())
    {
      // there are remaining mandatory parameters without matching command line argument
      return false;
    }

    return true;
  }

  void printUsage(const std::string& argv0)
  {
    // extract program name from argv[0]
    const auto executableName = std::filesystem::path(argv0).stem().string();
    // now the first line is a concatenation of the program name and the mandatory and optional parameters
    // if the options are not empty, they are also mentioned
    std::cout << executableName << " ";
    std::for_each(m_mandatoryParameters.begin(), m_mandatoryParameters.end(), [&](const auto& i) 
      {std::cout << "<" + i.name + "> "; });
    std::for_each(m_optionalParameters.begin(), m_optionalParameters.end(), [&](const auto& i)
      {std::cout << "[" + i.name + "] "; });
    if (!m_parserFunctions.empty() || !m_flags.empty()) std::cout << "[options...]";
    std::cout << std::endl;

    // now all option strings are generated
    std::vector<std::pair<std::string, std::string>> optionsAndDescriptions;
    std::transform(m_mandatoryParameters.begin(), m_mandatoryParameters.end(), std::back_inserter(optionsAndDescriptions),
      [](const auto& i) {return std::make_pair("<" + i.name + ">", i.description); });
    std::transform(m_optionalParameters.begin(), m_optionalParameters.end(), std::back_inserter(optionsAndDescriptions),
      [](const auto& i) {return std::make_pair("[" + i.name + "]", i.description); });
    std::transform(m_flags.begin(), m_flags.end(), std::back_inserter(optionsAndDescriptions),
      [](const auto& i) {return std::make_pair("-" + i.first, i.second.description); });
    std::transform(m_parserFunctions.begin(), m_parserFunctions.end(), std::back_inserter(optionsAndDescriptions),
      [](const auto& i) {return std::make_pair("-" + i.first + " <" + i.second.name + ">", i.second.description); });

    // now the options and parameters are described
    if (!optionsAndDescriptions.empty())
    {
      auto maxOptionStringLength = std::max_element(optionsAndDescriptions.begin(), optionsAndDescriptions.end(), 
        [](const auto& l, const auto& r) {return l.first.length() < r.first.length(); })->first.length();
      std::cout << std::endl << "Options" << std::endl;
      std::for_each(optionsAndDescriptions.begin(), optionsAndDescriptions.end(), [&](const auto& i)
        {std::cout << "  " << std::left << std::setw(maxOptionStringLength) << i.first << " " << i.second << std::endl; });
    }
  }

private:
  struct Parameter
  {
    std::function<bool(const std::string&)> parserFunction;
    std::string name, description;
  };

  struct Switch
  {
    bool* flagPointer;
    std::string description;
  };

  std::vector<Parameter> m_mandatoryParameters, m_optionalParameters;
  std::map<std::string, Parameter> m_parserFunctions;
  std::map<std::string, Switch> m_flags;
};