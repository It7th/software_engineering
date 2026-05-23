#include "events.hpp"

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

namespace {

bool findJsonValue(const Poco::Dynamic::Var& current, const std::string& key, Poco::Dynamic::Var& value);

} // namespace

std::string envOrDefault(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value && *value ? std::string(value) : fallback;
}

int envIntOrDefault(const char* name, int fallback) {
  const char* value = std::getenv(name);
  if (!value || !*value) {
    return fallback;
  }
  return std::stoi(value);
}

std::string nowIso() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
  std::tm time{};

#if defined(_WIN32)
  gmtime_s(&time, &timestamp);
#else
  gmtime_r(&timestamp, &time);
#endif

  std::ostringstream stream;
  stream << std::put_time(&time, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

std::string jsonEscape(const std::string& value) {
  std::string result;
  result.reserve(value.size() + 8);

  for (char c : value) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      result.push_back(c);
      break;
    }
  }

  return result;
}

std::string extractJsonString(const std::string& json, const std::string& key) {
  try {
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var root = parser.parse(json);
    Poco::Dynamic::Var value;
    if (findJsonValue(root, key, value) && !value.isEmpty()) {
      return value.convert<std::string>();
    }
  } catch (...) {
  }

  return "";
}

int extractJsonInt(const std::string& json, const std::string& key, int fallback) {
  try {
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var root = parser.parse(json);
    Poco::Dynamic::Var value;
    if (findJsonValue(root, key, value) && !value.isEmpty()) {
      return value.convert<int>();
    }
  } catch (...) {
  }

  return fallback;
}

namespace {

bool findJsonValue(const Poco::Dynamic::Var& current, const std::string& key, Poco::Dynamic::Var& value) {
  if (current.type() == typeid(Poco::JSON::Object::Ptr)) {
    Poco::JSON::Object::Ptr object = current.extract<Poco::JSON::Object::Ptr>();
    if (object->has(key)) {
      value = object->get(key);
      return true;
    }

    std::vector<std::string> names;
    object->getNames(names);
    for (const std::string& name : names) {
      if (findJsonValue(object->get(name), key, value)) {
        return true;
      }
    }
  }

  if (current.type() == typeid(Poco::JSON::Array::Ptr)) {
    Poco::JSON::Array::Ptr array = current.extract<Poco::JSON::Array::Ptr>();
    for (size_t index = 0; index < array->size(); ++index) {
      if (findJsonValue(array->get(index), key, value)) {
        return true;
      }
    }
  }

  return false;
}

std::string newId(const std::string& prefix) {
  static std::mt19937_64 generator{std::random_device{}()};
  const auto now = std::chrono::system_clock::now().time_since_epoch().count();
  std::uniform_int_distribution<unsigned long long> distribution;

  std::ostringstream stream;
  stream << prefix << "-" << std::hex << now << "-" << distribution(generator);
  return stream.str();
}

} // namespace

std::string makeEvent(
    const std::string& eventType,
    const std::string& routingKey,
    const std::string& commandName,
    const std::string& payload) {
  Poco::JSON::Parser parser;
  Poco::Dynamic::Var payloadObject = parser.parse(payload);

  Poco::JSON::Object event;
  event.set("eventId", newId("evt"));
  event.set("eventType", eventType);
  event.set("version", 1);
  event.set("occurredAt", nowIso());
  event.set("source", "lms-api");
  event.set("commandId", newId("cmd"));
  event.set("commandName", commandName);
  event.set("correlationId", "demo-flow-001");
  event.set("routingKey", routingKey);
  event.set("payload", payloadObject);

  std::ostringstream stream;
  event.stringify(stream);
  return stream.str();
}
