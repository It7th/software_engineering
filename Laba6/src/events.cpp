#include "events.hpp"

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

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
  const std::string marker = "\"" + key + "\"";
  size_t position = json.find(marker);
  if (position == std::string::npos) {
    return "";
  }

  position = json.find(':', position + marker.size());
  if (position == std::string::npos) {
    return "";
  }

  position = json.find('"', position + 1);
  if (position == std::string::npos) {
    return "";
  }

  std::string value;
  bool escaping = false;
  for (size_t i = position + 1; i < json.size(); ++i) {
    const char c = json[i];
    if (escaping) {
      switch (c) {
      case 'n':
        value.push_back('\n');
        break;
      case 'r':
        value.push_back('\r');
        break;
      case 't':
        value.push_back('\t');
        break;
      default:
        value.push_back(c);
        break;
      }
      escaping = false;
      continue;
    }

    if (c == '\\') {
      escaping = true;
      continue;
    }
    if (c == '"') {
      break;
    }
    value.push_back(c);
  }

  return value;
}

int extractJsonInt(const std::string& json, const std::string& key, int fallback) {
  const std::string marker = "\"" + key + "\"";
  size_t position = json.find(marker);
  if (position == std::string::npos) {
    return fallback;
  }

  position = json.find(':', position + marker.size());
  if (position == std::string::npos) {
    return fallback;
  }

  ++position;
  while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position]))) {
    ++position;
  }

  size_t end = position;
  while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-')) {
    ++end;
  }

  if (end == position) {
    return fallback;
  }

  return std::stoi(json.substr(position, end - position));
}

namespace {

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
  return "{"
         "\"eventId\":\"" + newId("evt") + "\","
         "\"eventType\":\"" + jsonEscape(eventType) + "\","
         "\"version\":1,"
         "\"occurredAt\":\"" + nowIso() + "\","
         "\"source\":\"lms-api\","
         "\"commandId\":\"" + newId("cmd") + "\","
         "\"commandName\":\"" + jsonEscape(commandName) + "\","
         "\"correlationId\":\"demo-flow-001\","
         "\"routingKey\":\"" + jsonEscape(routingKey) + "\","
         "\"payload\":" + payload +
         "}";
}
