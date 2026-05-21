#pragma once

#include <string>

struct HttpResponse {
  int status = 0;
  std::string body;
};

class HttpClient {
public:
  HttpClient(std::string host, int port, std::string user, std::string password);

  HttpResponse get(const std::string& path) const;
  HttpResponse put(const std::string& path, const std::string& body) const;
  HttpResponse post(const std::string& path, const std::string& body) const;

private:
  std::string host_;
  int port_;
  std::string authHeader_;

  HttpResponse request(const std::string& method, const std::string& path, const std::string& body) const;
};
