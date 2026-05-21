#pragma once

#include "http_client.hpp"

#include <optional>
#include <string>

class RabbitMqClient {
public:
  RabbitMqClient(std::string host, int port, std::string user, std::string password);

  void waitUntilReady() const;
  void declareTopology() const;
  bool publish(const std::string& routingKey, const std::string& payload) const;
  std::optional<std::string> getOne() const;

private:
  HttpClient http_;

  void expectOk(const HttpResponse& response, const std::string& action) const;
};
