#pragma once

#include <rabbitmq-c/amqp.h>

#include <optional>
#include <string>

class RabbitMqClient {
public:
  RabbitMqClient(std::string host, int port, std::string user, std::string password);
  ~RabbitMqClient();

  void waitUntilReady() const;
  void declareTopology() const;
  bool publish(const std::string& routingKey, const std::string& payload) const;
  std::optional<std::string> getOne() const;

private:
  std::string host_;
  int port_;
  std::string user_;
  std::string password_;
  mutable amqp_connection_state_t connection_ = nullptr;
  mutable bool connected_ = false;

  void connect() const;
  void close() const;
  void requireConnected() const;
};
