#pragma once

#include <string>

std::string envOrDefault(const char* name, const std::string& fallback);
int envIntOrDefault(const char* name, int fallback);

std::string nowIso();
std::string jsonEscape(const std::string& value);
std::string extractJsonString(const std::string& json, const std::string& key);
int extractJsonInt(const std::string& json, const std::string& key, int fallback = 0);

std::string makeEvent(
    const std::string& eventType,
    const std::string& routingKey,
    const std::string& commandName,
    const std::string& payload);
