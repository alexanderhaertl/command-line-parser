/* Copyright (C) 2020 Alexander Haertl - All rights reserved 
 * Distributed under the Boost Software License, Version 1.0.
 * See http://www.boost.org/LICENSE_1_0.txt
 */

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
#include <optional>

/* Helpers for string parsing of standard types, i.e. all types
 * that provide a formatted streaming operator (<< and >>)
 */
namespace StringParsing
{
  /** Tries to parse the value provided as string into the value. If parsing
    * fails, an std::invalid_argument exception is thrown.
    * @tparam T The type of the value to be parsed.
    * @param valueAsString The string representation of the value to be parsed
    * @param value The output value containing the parsed value on success.
    * @exception std::invalid_argument if parsing is not successful.
    */
  template <typename T> void parseString(const std::string& valueAsString, T& value)
  {
    // expect that the state is exactly eof, which means everything could be parsed and no error happened
    if ((std::stringstream(valueAsString) >> value).rdstate() != std::ios::eofbit)
    {
      throw std::invalid_argument("Error parsing value " + valueAsString + " as " + std::string(typeid(T).name()));
    }
  }

  /** Specialization of above function template for std::optional.
    */
  template <typename T> void parseString(const std::string& valueAsString, std::optional<T>& value)
  {
    T temp;
    parseString(valueAsString, temp);
    value = temp;
  }

  /** Specialization of above function template for std::string, which always succeeds.
    * The valueAsString is simply copied to value.
    */
  template <> void parseString(const std::string& valueAsString, std::string& value)
  {
    value = valueAsString;
  }

  /** Prints the provided value to a stream.
    * @tparam The type of the value to be printed.
    * @param value The value to be printed.
    * @param stream The stream to which the value is printed.
    */
  template <typename T> std::string printValue(const T& value)
  {
    return (std::stringstream() << ", default:" << value).str();
  }

  /** Specialization of above template for std::optional. If the optional value
    * does not contain an actual value, nothing is printed.
    */
  template <typename T> std::string printValue(const std::optional<T>& value)
  {
    return value.has_value() ? printValue(*value) : "";
  }
};

/** Class for parsing the command line arguments. It is to be used as follows:
  * After instanciating this class, all parameters that should be mapped to command
  * line arguments are registered. The mapped parameters are provided as references
  * that must remain valid until parsing is finished. Then the command line arguments
  * are used to do the actual parsing. Named options are identified by a dash ('-')
  * followed by the option identifier. Errors in parsing are reported by std::invalid_argument
  * exception. A usage screen can be generated automatically based on the information and parameters
  * provided. This implies that the entire command line parsing is done by this class.
  */
class CommandLineOptionParser
{
public:
  /** Template function for registering parameters mapped to command line arguments. Works for
    * all standard types for which standard streaming operators are defined. The parameter
    * provided is stored as reference and changed when \ref parseCommandLineArguments is called.
    * The parameter is expected to be reasonably initialized for the case that the option is not set.
    * @tparam T Type of the parameter to be registered.
    * @param parameter Reference to the parameter mapped to a command line argument.
    * @param optionIdentifier The identifier for the option. Implicitly preceded with a dash ('-')
    * @param parameterIdentifier Name printed after the option identifier.
    * @param description Longer description text printed in usage.
    */
  template <typename T>
  void registerOption(T& parameter, const std::string& optionIdentifier, const std::string& parameterName, const std::string& description)
  {
    m_namedOptions.emplace(optionIdentifier, Parameter({
      [&](const std::string& valueAsString) { StringParsing::parseString(valueAsString, parameter); },
      [&] { return StringParsing::printValue(parameter); },
      parameterName,
      description }));
  }

  /** Registers a boolean flag or switch that is mapped to a command line argument. If the command
    * line option is set, the boolean value is negated. The parameter is expected to be reasonably
    * initialized for the case that the option is not set.
    * @param parameter Reference to the boolean parameter that should be mapped.
    * @param parameterIdentifier The identifier for the parameter. Implicitly preceded with a dash ('-')
    * @param description Longer description text printed in usage.
    */
  void registerSwitch(bool& parameter, const std::string& parameterIdentifier, const std::string& description)
  {
    m_flags.emplace(parameterIdentifier, Switch({ &parameter, description }));
  }

  /** Registers an unnamed parameter, i.e. a parameter that must not be preceeded by
    * a dash and an option identifier. Parameters can be either mandatory or optional.
    * Optional parameters are always put after the mandatory parameters, regardless of
    * the order of registration. The order with in the group of mandatory and optional
    * parameters, respectively, is determined by the order of registration.
    * @tparam T Type of the parameter to be registered.
    * @param parameter Reference to the parameter that should be mapped.
    * @param parameterIdentifier The identifier for the parameter (only used to be printed in usage).
    * @param description Longer description text printed in usage.
    */
  template <typename T>
  void registerUnnamedParameter(T& parameter, const std::string& parameterName, const std::string& description, bool mandatory = true)
  {
    (mandatory ? m_mandatoryParameters : m_optionalParameters).push_back({
      [&](const std::string& valueAsString) { StringParsing::parseString(valueAsString, parameter);},
      [&] { return StringParsing::printValue(parameter); },
      parameterName,
      description});
  }

  /** Parses the provided command line arguments and matches them against the registered
    * parameters.
    * @param args Command line arguments as vector of strings
    * @exception std::invalid_argument If the matching process fails.
    * - if a registered parameter can not be parsed to the value type provided
    * - if an unknown option is provided
    * - if an unnamed option is not matched by a registered parameter
    * - if an option is not followed by a valid corresponding value
    */
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

  /** Overload for providing directly the typical main function arguments */
  void parseCommandLineArguments(int argc, char** argv)
  {
    parseCommandLineArguments(std::vector<std::string>(argv, argv + argc));
  }

  /** Generates a classical usage screen for console programs and prints it to the stream provided.
    * The program name is extracted from the first command line argument.
    * By default, the usage is printed to standard output.
    * @param argv0 The first command line argument usually containing the full path to the running executable.
    * @param stream The stream (extending std::ostream) to which the usage is printed.
    * @param printDefaultValues Whether the default values (current value of the registered variables during parsing) shall be printed
    */
  void printUsage(const std::string& argv0, std::ostream& stream = std::cout, bool printDefaultValues = false)
  {
    // extract program name from argv[0]
    const auto executableName = std::filesystem::path(argv0).stem().string();
    // now the first line is a concatenation of the program name and the mandatory and optional parameters
    // if the options are not empty, they are also mentioned
    stream << "usage: " << executableName << " ";
    std::for_each(m_mandatoryParameters.begin(), m_mandatoryParameters.end(), [&](const auto& i) 
      {stream << "<" + i.name + "> "; });
    std::for_each(m_optionalParameters.begin(), m_optionalParameters.end(), [&](const auto& i)
      {stream << "[" + i.name + "] "; });
    if (!m_namedOptions.empty() || !m_flags.empty()) stream << "[options...]";
    stream << std::endl;

    // now all option strings are generated
    std::vector<TextualDescription> textualDescriptions;
    std::transform(m_mandatoryParameters.begin(), m_mandatoryParameters.end(), std::back_inserter(textualDescriptions),
      [](const auto& i) {return TextualDescription({ "<" + i.name + ">", i.description, i.printFunction() }); });
    std::transform(m_optionalParameters.begin(), m_optionalParameters.end(), std::back_inserter(textualDescriptions),
      [](const auto& i) {return TextualDescription({ "[" + i.name + "]", i.description, i.printFunction() }); });
    std::transform(m_flags.begin(), m_flags.end(), std::back_inserter(textualDescriptions),
      [](const auto& i) {return TextualDescription({ "-" + i.first, i.second.description, std::string(", default: ") + (*i.second.flagPointer ? "true" : "false") }); });
    std::transform(m_namedOptions.begin(), m_namedOptions.end(), std::back_inserter(textualDescriptions),
      [](const auto& i) {return TextualDescription({ "-" + i.first + " <" + i.second.name + ">", i.second.description, i.second.printFunction() }); });

    // now the options and parameters are described
    if (!textualDescriptions.empty())
    {
      auto maxOptionStringLength = std::max_element(textualDescriptions.begin(), textualDescriptions.end(), 
        [](const auto& l, const auto& r) {return l.optionString.length() < r.optionString.length(); })->optionString.length();
      stream << std::endl << "options" << std::endl;
      std::for_each(textualDescriptions.begin(), textualDescriptions.end(), [&](const auto& i)
        {stream << "  " << std::left << std::setw(maxOptionStringLength) << i.optionString << " " << i.description;
      if(printDefaultValues) stream << i.defaultValue << std::endl; });
    }
  }

private:
  struct Parameter
  {
    std::function<void(const std::string&)> parserFunction;
    std::function<std::string()> printFunction;
    std::string name, description;
  };

  struct Switch
  {
    bool* flagPointer;
    std::string description;
  };

  struct TextualDescription
  {
    std::string optionString, description, defaultValue;
  };

  std::vector<Parameter> m_mandatoryParameters, m_optionalParameters;
  std::map<std::string, Parameter> m_namedOptions;
  std::map<std::string, Switch> m_flags;
};