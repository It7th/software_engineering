#include "rabbitmq_client.hpp"

#include "events.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

const char* exchangePath = "/api/exchanges/%2F/lms.events";
const char* queuePath = "/api/queues/%2F/lms.projections";
const char* bindingPath = "/api/bindings/%2F/e/lms.events/q/lms.projections";

bool isSuccess(int status) {
  return status >= 200 && status < 300;
}

} // namespace

RabbitMqClient::RabbitMqClient(std::string host, int port, std::string user, std::string password)
    : http_(std::move(host), port, std::move(user), std::move(password)) {
}

void RabbitMqClient::waitUntilReady() const {
  for (int attempt = 1; attempt <= 30; ++attempt) {
    try {
      const HttpResponse response = http_.get("/api/overview");
      if (isSuccess(response.status)) {
        return;
      }
    } catch (const std::exception&) {
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  throw std::runtime_error("RabbitMQ management API is unavailable");
}

void RabbitMqClient::declareTopology() const {
  expectOk(http_.put(exchangePath, "{\"type\":\"topic\",\"durable\":true,\"auto_delete\":false,\"arguments\":{}}"), "declare exchange");
  expectOk(http_.put(queuePath, "{\"durable\":true,\"auto_delete\":false,\"arguments\":{}}"), "declare queue");
  expectOk(http_.post(bindingPath, "{\"routing_key\":\"#\",\"arguments\":{}}"), "bind queue");
}

bool RabbitMqClient::publish(const std::string& routingKey, const std::string& payload) const {
  const std::string requestBody =
      "{"
      "\"properties\":{\"delivery_mode\":2,\"content_type\":\"application/json\"},"
      "\"routing_key\":\"" + jsonEscape(routingKey) + "\","
      "\"payload\":\"" + jsonEscape(payload) + "\","
      "\"payload_encoding\":\"string\""
      "}";

  const HttpResponse response = http_.post("/api/exchanges/%2F/lms.events/publish", requestBody);
  expectOk(response, "publish event");
  return response.body.find("\"routed\":true") != std::string::npos;
}

std::optional<std::string> RabbitMqClient::getOne() const {
  const std::string requestBody = "{\"count\":1,\"ackmode\":\"ack_requeue_false\",\"encoding\":\"auto\",\"truncate\":50000}";
  const HttpResponse response = http_.post("/api/queues/%2F/lms.projections/get", requestBody);
  expectOk(response, "get message");

  if (response.body == "[]" || response.body.find("\"payload\"") == std::string::npos) {
    return std::nullopt;
  }

  return extractJsonString(response.body, "payload");
}

void RabbitMqClient::expectOk(const HttpResponse& response, const std::string& action) const {
  if (!isSuccess(response.status)) {
    throw std::runtime_error(action + " failed with HTTP " + std::to_string(response.status) + ": " + response.body);
  }
}
