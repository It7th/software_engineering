#include "rabbitmq_client.hpp"

#include <rabbitmq-c/framing.h>
#include <rabbitmq-c/tcp_socket.h>

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

constexpr amqp_channel_t channelId = 1;
const char* exchangeName = "lms.events";
const char* queueName = "lms.projections";

std::string amqpBytesToString(amqp_bytes_t bytes) {
  return std::string(static_cast<const char*>(bytes.bytes), bytes.len);
}

std::string replyText(amqp_rpc_reply_t reply) {
  if (reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
    return amqp_error_string2(reply.library_error);
  }

  if (reply.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION) {
    if (reply.reply.id == AMQP_CONNECTION_CLOSE_METHOD) {
      const auto* method = static_cast<amqp_connection_close_t*>(reply.reply.decoded);
      return "connection close: " + amqpBytesToString(method->reply_text);
    }
    if (reply.reply.id == AMQP_CHANNEL_CLOSE_METHOD) {
      const auto* method = static_cast<amqp_channel_close_t*>(reply.reply.decoded);
      return "channel close: " + amqpBytesToString(method->reply_text);
    }
    return "server exception";
  }

  if (reply.reply_type == AMQP_RESPONSE_NONE) {
    return "missing rpc reply";
  }

  return "ok";
}

void requireReply(amqp_rpc_reply_t reply, const std::string& action) {
  if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
    throw std::runtime_error(action + " failed: " + replyText(reply));
  }
}

void requireStatus(int status, const std::string& action) {
  if (status < 0) {
    throw std::runtime_error(action + " failed: " + std::string(amqp_error_string2(status)));
  }
}

} // namespace

RabbitMqClient::RabbitMqClient(std::string host, int port, std::string user, std::string password)
    : host_(std::move(host)), port_(port), user_(std::move(user)), password_(std::move(password)) {
}

RabbitMqClient::~RabbitMqClient() {
  close();
}

void RabbitMqClient::waitUntilReady() const {
  for (int attempt = 1; attempt <= 30; ++attempt) {
    try {
      connect();
      return;
    } catch (const std::exception&) {
      close();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  throw std::runtime_error("RabbitMQ AMQP endpoint is unavailable");
}

void RabbitMqClient::declareTopology() const {
  requireConnected();

  amqp_exchange_declare(
      connection_,
      channelId,
      amqp_cstring_bytes(exchangeName),
      amqp_cstring_bytes("topic"),
      0,
      1,
      0,
      0,
      amqp_empty_table);
  requireReply(amqp_get_rpc_reply(connection_), "declare exchange");

  amqp_queue_declare(
      connection_,
      channelId,
      amqp_cstring_bytes(queueName),
      0,
      1,
      0,
      0,
      amqp_empty_table);
  requireReply(amqp_get_rpc_reply(connection_), "declare queue");

  amqp_queue_bind(
      connection_,
      channelId,
      amqp_cstring_bytes(queueName),
      amqp_cstring_bytes(exchangeName),
      amqp_cstring_bytes("#"),
      amqp_empty_table);
  requireReply(amqp_get_rpc_reply(connection_), "bind queue");
}

bool RabbitMqClient::publish(const std::string& routingKey, const std::string& payload) const {
  requireConnected();

  amqp_basic_properties_t properties{};
  properties._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
  properties.content_type = amqp_cstring_bytes("application/json");
  properties.delivery_mode = 2;

  const int status = amqp_basic_publish(
      connection_,
      channelId,
      amqp_cstring_bytes(exchangeName),
      amqp_cstring_bytes(routingKey.c_str()),
      0,
      0,
      &properties,
      amqp_cstring_bytes(payload.c_str()));

  requireStatus(status, "publish event");
  return true;
}

std::optional<std::string> RabbitMqClient::getOne() const {
  requireConnected();

  amqp_basic_get(connection_, channelId, amqp_cstring_bytes(queueName), 0);
  amqp_rpc_reply_t reply = amqp_get_rpc_reply(connection_);
  requireReply(reply, "get message");

  if (reply.reply.id == AMQP_BASIC_GET_EMPTY_METHOD) {
    return std::nullopt;
  }
  if (reply.reply.id != AMQP_BASIC_GET_OK_METHOD) {
    throw std::runtime_error("unexpected basic.get reply");
  }

  const auto* getOk = static_cast<amqp_basic_get_ok_t*>(reply.reply.decoded);
  amqp_message_t message{};
  requireReply(amqp_read_message(connection_, channelId, &message, 0), "read message body");

  std::string body(static_cast<const char*>(message.body.bytes), message.body.len);
  amqp_destroy_message(&message);

  requireStatus(amqp_basic_ack(connection_, channelId, getOk->delivery_tag, 0), "ack message");
  return body;
}

void RabbitMqClient::connect() const {
  if (connected_) {
    return;
  }

  connection_ = amqp_new_connection();
  amqp_socket_t* socket = amqp_tcp_socket_new(connection_);
  if (socket == nullptr) {
    throw std::runtime_error("cannot create AMQP TCP socket");
  }

  requireStatus(amqp_socket_open(socket, host_.c_str(), port_), "open AMQP socket");
  requireReply(
      amqp_login(
          connection_,
          "/",
          0,
          131072,
          0,
          AMQP_SASL_METHOD_PLAIN,
          user_.c_str(),
          password_.c_str()),
      "login");

  amqp_channel_open(connection_, channelId);
  requireReply(amqp_get_rpc_reply(connection_), "open channel");
  connected_ = true;
}

void RabbitMqClient::close() const {
  if (connection_ == nullptr) {
    connected_ = false;
    return;
  }

  if (connected_) {
    amqp_channel_close(connection_, channelId, AMQP_REPLY_SUCCESS);
    amqp_connection_close(connection_, AMQP_REPLY_SUCCESS);
  }

  amqp_destroy_connection(connection_);
  connection_ = nullptr;
  connected_ = false;
}

void RabbitMqClient::requireConnected() const {
  if (!connected_) {
    connect();
  }
}
