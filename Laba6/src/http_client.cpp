#include "http_client.hpp"

#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace {

std::string base64Encode(const std::string& value) {
  static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  int valueBuffer = 0;
  int bits = -6;

  for (unsigned char c : value) {
    valueBuffer = (valueBuffer << 8) + c;
    bits += 8;
    while (bits >= 0) {
      result.push_back(alphabet[(valueBuffer >> bits) & 0x3F]);
      bits -= 6;
    }
  }

  if (bits > -6) {
    result.push_back(alphabet[((valueBuffer << 8) >> (bits + 8)) & 0x3F]);
  }
  while (result.size() % 4 != 0) {
    result.push_back('=');
  }

  return result;
}

int connectToHost(const std::string& host, int port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* addresses = nullptr;
  const std::string portText = std::to_string(port);
  const int error = getaddrinfo(host.c_str(), portText.c_str(), &hints, &addresses);
  if (error != 0) {
    throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(error));
  }

  int socketFd = -1;
  for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
    socketFd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (socketFd < 0) {
      continue;
    }
    if (connect(socketFd, address->ai_addr, address->ai_addrlen) == 0) {
      break;
    }
    close(socketFd);
    socketFd = -1;
  }

  freeaddrinfo(addresses);
  if (socketFd < 0) {
    throw std::runtime_error("cannot connect to " + host + ":" + portText);
  }

  return socketFd;
}

void sendAll(int socketFd, const std::string& data) {
  const char* cursor = data.data();
  size_t left = data.size();

  while (left > 0) {
    const ssize_t sent = send(socketFd, cursor, left, 0);
    if (sent <= 0) {
      throw std::runtime_error("send failed");
    }
    cursor += sent;
    left -= static_cast<size_t>(sent);
  }
}

std::string receiveAll(int socketFd) {
  std::string response;
  char buffer[4096];

  while (true) {
    const ssize_t count = recv(socketFd, buffer, sizeof(buffer), 0);
    if (count < 0) {
      throw std::runtime_error("recv failed");
    }
    if (count == 0) {
      break;
    }
    response.append(buffer, static_cast<size_t>(count));
  }

  return response;
}

int parseStatus(const std::string& raw) {
  const size_t firstSpace = raw.find(' ');
  if (firstSpace == std::string::npos || firstSpace + 4 > raw.size()) {
    return 0;
  }
  return std::stoi(raw.substr(firstSpace + 1, 3));
}

std::string parseBody(const std::string& raw) {
  const size_t headerEnd = raw.find("\r\n\r\n");
  if (headerEnd == std::string::npos) {
    return "";
  }
  return raw.substr(headerEnd + 4);
}

} // namespace

HttpClient::HttpClient(std::string host, int port, std::string user, std::string password)
    : host_(std::move(host)), port_(port), authHeader_("Basic " + base64Encode(user + ":" + password)) {
}

HttpResponse HttpClient::get(const std::string& path) const {
  return request("GET", path, "");
}

HttpResponse HttpClient::put(const std::string& path, const std::string& body) const {
  return request("PUT", path, body);
}

HttpResponse HttpClient::post(const std::string& path, const std::string& body) const {
  return request("POST", path, body);
}

HttpResponse HttpClient::request(const std::string& method, const std::string& path, const std::string& body) const {
  std::string requestText =
      method + " " + path + " HTTP/1.1\r\n" +
      "Host: " + host_ + "\r\n" +
      "Authorization: " + authHeader_ + "\r\n" +
      "Accept: application/json\r\n" +
      "Content-Type: application/json\r\n" +
      "Content-Length: " + std::to_string(body.size()) + "\r\n" +
      "Connection: close\r\n\r\n" +
      body;

  const int socketFd = connectToHost(host_, port_);
  try {
    sendAll(socketFd, requestText);
    const std::string rawResponse = receiveAll(socketFd);
    close(socketFd);
    return {parseStatus(rawResponse), parseBody(rawResponse)};
  } catch (...) {
    close(socketFd);
    throw;
  }
}
