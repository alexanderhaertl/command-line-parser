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
#include <typeinfo>

namespace StringParsing
{
  template <typename T> void parseString(const std::string& valueAsString, T& value)
  {
    // expect that the state is exactly eof, which means everything could be parsed and no error happened
    if ((std::stringstream(valueAsString) >> value).rdstate() != std::ios::eofbit)
    {
      throw std::invalid_argument("Error parsing value " + valueAsString + " as " + std::string(typeid(T).name()));
    }
  }

  template <> void parseString(const std::string& valueAsString, std::string& value)
  {
    value = valueAsString;
  }
};

class CommandLineOptionParser
{
public:
  template <typename T>
  void registerOption(T& parameter, const std::string& optionName, const std::string& parameterName, const std::string& description)
  {
    m_namedOptions.emplace(optionName, Parameter({
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

  void parseCommandLineArguments(int argc, char** argv)
  {
    parseCommandLineArguments(std::vector<std::string>(argv, argv + argc));
  }

  void parseCommandLineArguments(const std::vector<std::string>& args) const
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
        auto paramFindResult = m_namedOptions.find(optionIdentifier);
        if (flagFindResult != m_flags.end())
        {
          auto& flag = *flagFindResult->second.flagPointer;
          flag = !flag;
        }
        else if (paramFindResult != m_namedOptions.end())
        {
          if (++i != args.end()) // we have at least one parameter left
          {
            // parameter iterator was intentionally advanced, because the actual option is the parameter following the option identifier
            paramFindResult->second.parserFunction(*i);
          }
          else
          {
            // we have an option expecting a parameter, without a parameter following
            throw std::invalid_argument("Option " + *(--i) + " not followed by value <" + paramFindResult->second.name + ">");
          }
        }
        else
        {
          // unknown option
          throw std::invalid_argument("Unknown option " + *i);
        }
      }
      else
      {
        // unnamed option
        if (mandatoryParamIterator != m_mandatoryParameters.end())
        {
          // we have mandatory parameter to match
          mandatoryParamIterator++->parserFunction(*i);
        }
        else if (optionalParamIterator != m_optionalParameters.end())
        {
          // we have an optional parameter to match
          optionalParamIterator++->parserFunction(*i);
        }
        else
        {
          // we have an unnamed parameter without registered counterpart
          throw std::invalid_argument("Parameter " + *i + " not recognized");
        }
      }
    }

    // check that all mandatory parameters are matched
    if (mandatoryParamIterator != m_mandatoryParameters.end())
    {
      // there are remaining mandatory parameters without matching command line argument
      throw std::invalid_argument("Mandatory parameter " + mandatoryParamIterator->name + " not provided");
    }
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
    if (!m_namedOptions.empty() || !m_flags.empty()) std::cout << "[options...]";
    std::cout << std::endl;

    // now all option strings are generated
    std::vector<std::pair<std::string, std::string>> optionsAndDescriptions;
    std::transform(m_mandatoryParameters.begin(), m_mandatoryParameters.end(), std::back_inserter(optionsAndDescriptions),
      [](const auto& i) {return std::make_pair("<" + i.name + ">", i.description); });
    std::transform(m_optionalParameters.begin(), m_optionalParameters.end(), std::back_inserter(optionsAndDescriptions),
      [](const auto& i) {return std::make_pair("[" + i.name + "]", i.description); });
    std::transform(m_flags.begin(), m_flags.end(), std::back_inserter(optionsAndDescriptions),
      [](const auto& i) {return std::make_pair("-" + i.first, i.second.description); });
    std::transform(m_namedOptions.begin(), m_namedOptions.end(), std::back_inserter(optionsAndDescriptions),
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
    std::function<void(const std::string&)> parserFunction;
    std::string name, description;
  };

  struct Switch
  {
    bool* flagPointer;
    std::string description;
  };

  std::vector<Parameter> m_mandatoryParameters, m_optionalParameters;
  std::map<std::string, Parameter> m_namedOptions;
  std::map<std::string, Switch> m_flags;
};