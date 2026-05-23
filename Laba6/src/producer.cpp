#include "events.hpp"
#include "rabbitmq_client.hpp"

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/URI.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

struct User {
  std::string id;
  std::string login;
  std::string firstName;
  std::string lastName;
  std::string email;
  std::string role;
  std::string status;
};

struct Course {
  std::string id;
  std::string title;
  std::string description;
  std::string authorId;
  std::string status;
};

struct Lesson {
  std::string id;
  std::string courseId;
  std::string title;
  int position = 0;
  std::string status;
};

struct Enrollment {
  std::string id;
  std::string userId;
  std::string courseId;
  std::string status;
};

std::string toJson(const Poco::JSON::Object& object) {
  std::ostringstream stream;
  object.stringify(stream);
  return stream.str();
}

std::string bodyToString(std::istream& stream) {
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

Poco::JSON::Object::Ptr parseBody(std::istream& stream) {
  const std::string body = bodyToString(stream);
  if (body.empty()) {
    return new Poco::JSON::Object();
  }

  Poco::JSON::Parser parser;
  return parser.parse(body).extract<Poco::JSON::Object::Ptr>();
}

std::string field(Poco::JSON::Object::Ptr object, const std::string& name, const std::string& fallback = "") {
  if (!object->has(name) || object->isNull(name)) {
    return fallback;
  }
  return object->getValue<std::string>(name);
}

int intField(Poco::JSON::Object::Ptr object, const std::string& name, int fallback = 0) {
  if (!object->has(name) || object->isNull(name)) {
    return fallback;
  }
  return object->getValue<int>(name);
}

std::vector<std::string> splitPath(const std::string& path) {
  std::vector<std::string> parts;
  std::stringstream stream(path);
  std::string item;
  while (std::getline(stream, item, '/')) {
    if (!item.empty()) {
      parts.push_back(item);
    }
  }
  return parts;
}

std::string queryParam(const Poco::URI& uri, const std::string& name) {
  for (const auto& item : uri.getQueryParameters()) {
    if (item.first == name) {
      return item.second;
    }
  }
  return "";
}

Poco::JSON::Object userJson(const User& user) {
  Poco::JSON::Object object;
  object.set("id", user.id);
  object.set("login", user.login);
  object.set("firstName", user.firstName);
  object.set("lastName", user.lastName);
  object.set("email", user.email);
  object.set("role", user.role);
  object.set("status", user.status);
  return object;
}

Poco::JSON::Object courseJson(const Course& course) {
  Poco::JSON::Object object;
  object.set("id", course.id);
  object.set("title", course.title);
  object.set("description", course.description);
  object.set("authorId", course.authorId);
  object.set("status", course.status);
  return object;
}

Poco::JSON::Object lessonJson(const Lesson& lesson) {
  Poco::JSON::Object object;
  object.set("id", lesson.id);
  object.set("courseId", lesson.courseId);
  object.set("title", lesson.title);
  object.set("position", lesson.position);
  object.set("status", lesson.status);
  return object;
}

Poco::JSON::Object enrollmentJson(const Enrollment& enrollment) {
  Poco::JSON::Object object;
  object.set("id", enrollment.id);
  object.set("userId", enrollment.userId);
  object.set("courseId", enrollment.courseId);
  object.set("status", enrollment.status);
  return object;
}

class LmsState {
public:
  explicit LmsState(RabbitMqClient& rabbit) : rabbit_(rabbit) {
  }

  Poco::JSON::Object createUser(Poco::JSON::Object::Ptr body) {
    std::lock_guard<std::mutex> lock(mutex_);
    User user;
    user.id = "user-" + std::to_string(nextUserId_++);
    user.login = field(body, "login");
    user.firstName = field(body, "firstName");
    user.lastName = field(body, "lastName");
    user.email = field(body, "email");
    user.role = field(body, "role", "STUDENT");
    user.status = field(body, "status", "ACTIVE");
    users_[user.id] = user;

    Poco::JSON::Object payload = userJson(user);
    payload.set("userId", user.id);
    publish("identity.user.created", "UserCreated", "CreateUser", payload);

    Poco::JSON::Object response;
    response.set("user", userJson(user));
    return response;
  }

  Poco::JSON::Object findUserByLogin(const std::string& login) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, user] : users_) {
      if (user.login == login) {
        Poco::JSON::Object response;
        response.set("user", userJson(user));
        return response;
      }
    }
    throw std::runtime_error("user not found");
  }

  Poco::JSON::Object searchUsers(const std::string& firstNameMask, const std::string& lastNameMask) {
    std::lock_guard<std::mutex> lock(mutex_);
    Poco::JSON::Array items;
    for (const auto& [id, user] : users_) {
      const bool firstOk = firstNameMask.empty() || user.firstName.find(firstNameMask) != std::string::npos;
      const bool lastOk = lastNameMask.empty() || user.lastName.find(lastNameMask) != std::string::npos;
      if (firstOk && lastOk) {
        items.add(userJson(user));
      }
    }

    Poco::JSON::Object response;
    response.set("count", static_cast<int>(items.size()));
    response.set("items", items);
    return response;
  }

  Poco::JSON::Object createCourse(Poco::JSON::Object::Ptr body) {
    std::lock_guard<std::mutex> lock(mutex_);
    Course course;
    course.id = "course-" + std::to_string(nextCourseId_++);
    course.title = field(body, "title");
    course.description = field(body, "description");
    course.authorId = field(body, "authorId", "user-900");
    course.status = field(body, "status", "PUBLISHED");
    courses_[course.id] = course;

    Poco::JSON::Object payload = courseJson(course);
    payload.set("courseId", course.id);
    publish("catalog.course.created", "CourseCreated", "CreateCourse", payload);

    Poco::JSON::Object response;
    response.set("course", courseJson(course));
    return response;
  }

  Poco::JSON::Object listCourses() {
    std::lock_guard<std::mutex> lock(mutex_);
    Poco::JSON::Array items;
    for (const auto& [id, course] : courses_) {
      items.add(courseJson(course));
    }

    Poco::JSON::Object response;
    response.set("count", static_cast<int>(items.size()));
    response.set("items", items);
    return response;
  }

  Poco::JSON::Object addLesson(const std::string& courseId, Poco::JSON::Object::Ptr body) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (courses_.count(courseId) == 0) {
      throw std::runtime_error("course not found");
    }

    Lesson lesson;
    lesson.id = "lesson-" + std::to_string(nextLessonId_++);
    lesson.courseId = courseId;
    lesson.title = field(body, "title");
    lesson.position = intField(body, "position", static_cast<int>(lessonsByCourse_[courseId].size()) + 1);
    lesson.status = field(body, "status", "PUBLISHED");
    lessons_[lesson.id] = lesson;
    lessonsByCourse_[courseId].push_back(lesson.id);

    Poco::JSON::Object payload = lessonJson(lesson);
    payload.set("lessonId", lesson.id);
    publish("catalog.lesson.added", "LessonAdded", "AddLessonToCourse", payload);

    Poco::JSON::Object response;
    response.set("lesson", lessonJson(lesson));
    return response;
  }

  Poco::JSON::Object listLessons(const std::string& courseId) {
    std::lock_guard<std::mutex> lock(mutex_);
    Poco::JSON::Array items;
    for (const std::string& lessonId : lessonsByCourse_[courseId]) {
      items.add(lessonJson(lessons_.at(lessonId)));
    }

    Poco::JSON::Object response;
    response.set("count", static_cast<int>(items.size()));
    response.set("items", items);
    return response;
  }

  Poco::JSON::Object enrollUser(const std::string& courseId, Poco::JSON::Object::Ptr body) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string userId = field(body, "userId");
    if (users_.count(userId) == 0 || courses_.count(courseId) == 0) {
      throw std::runtime_error("user or course not found");
    }
    if (enrollmentByUserCourse_.count({userId, courseId}) > 0) {
      throw std::runtime_error("user is already enrolled");
    }

    Enrollment enrollment;
    enrollment.id = "enrollment-" + std::to_string(nextEnrollmentId_++);
    enrollment.userId = userId;
    enrollment.courseId = courseId;
    enrollment.status = "ACTIVE";
    enrollments_[enrollment.id] = enrollment;
    enrollmentByUserCourse_[{userId, courseId}] = enrollment.id;

    Poco::JSON::Object payload = enrollmentJson(enrollment);
    payload.set("enrollmentId", enrollment.id);
    publish("learning.enrollment.created", "UserEnrolledInCourse", "EnrollUserInCourse", payload);

    Poco::JSON::Object response;
    response.set("enrollment", payload);
    return response;
  }

  Poco::JSON::Object listUserCourses(const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    Poco::JSON::Array items;
    for (const auto& [id, enrollment] : enrollments_) {
      if (enrollment.userId != userId) {
        continue;
      }

      Poco::JSON::Object item;
      item.set("enrollment", enrollmentJson(enrollment));
      item.set("course", courseJson(courses_.at(enrollment.courseId)));
      items.add(item);
    }

    Poco::JSON::Object response;
    response.set("count", static_cast<int>(items.size()));
    response.set("items", items);
    return response;
  }

  Poco::JSON::Object completeLesson(const std::string& userId, const std::string& lessonId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (users_.count(userId) == 0 || lessons_.count(lessonId) == 0) {
      throw std::runtime_error("user or lesson not found");
    }

    const Lesson& lesson = lessons_.at(lessonId);
    if (enrollmentByUserCourse_.count({userId, lesson.courseId}) == 0) {
      throw std::runtime_error("user is not enrolled in course");
    }

    completedLessons_.insert({userId, lessonId});

    Poco::JSON::Object payload;
    payload.set("userId", userId);
    payload.set("courseId", lesson.courseId);
    payload.set("lessonId", lessonId);
    publish("learning.lesson.completed", "LessonCompleted", "MarkLessonCompleted", payload);

    Poco::JSON::Object response;
    response.set("completion", payload);
    return response;
  }

private:
  RabbitMqClient& rabbit_;
  std::mutex mutex_;
  int nextUserId_ = 100;
  int nextCourseId_ = 200;
  int nextLessonId_ = 300;
  int nextEnrollmentId_ = 400;
  std::map<std::string, User> users_;
  std::map<std::string, Course> courses_;
  std::map<std::string, Lesson> lessons_;
  std::map<std::string, Enrollment> enrollments_;
  std::map<std::string, std::vector<std::string>> lessonsByCourse_;
  std::map<std::pair<std::string, std::string>, std::string> enrollmentByUserCourse_;
  std::set<std::pair<std::string, std::string>> completedLessons_;

  void publish(const std::string& routingKey, const std::string& eventType, const std::string& commandName, const Poco::JSON::Object& payload) {
    const std::string event = makeEvent(eventType, routingKey, commandName, toJson(payload));
    rabbit_.publish(routingKey, event);
    std::cout << "published " << eventType << " by " << routingKey << std::endl;
  }
};

class LmsHandler : public Poco::Net::HTTPRequestHandler {
public:
  explicit LmsHandler(LmsState& state) : state_(state) {
  }

  void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
    try {
      Poco::URI uri(request.getURI());
      const std::vector<std::string> parts = splitPath(uri.getPath());
      Poco::JSON::Object result;
      int status = Poco::Net::HTTPResponse::HTTP_OK;
      const std::string method = request.getMethod();

      if (method == "GET" && parts.size() == 1 && parts[0] == "health") {
        result.set("status", "ok");
        result.set("service", "lms-event-producer");
      } else if (method == "POST" && parts.size() == 1 && parts[0] == "users") {
        result = state_.createUser(parseBody(request.stream()));
        status = Poco::Net::HTTPResponse::HTTP_CREATED;
      } else if (method == "GET" && parts.size() == 2 && parts[0] == "users" && parts[1] == "by-login") {
        result = state_.findUserByLogin(queryParam(uri, "login"));
      } else if (method == "GET" && parts.size() == 2 && parts[0] == "users" && parts[1] == "search") {
        result = state_.searchUsers(queryParam(uri, "firstNameMask"), queryParam(uri, "lastNameMask"));
      } else if (method == "POST" && parts.size() == 1 && parts[0] == "courses") {
        result = state_.createCourse(parseBody(request.stream()));
        status = Poco::Net::HTTPResponse::HTTP_CREATED;
      } else if (method == "GET" && parts.size() == 1 && parts[0] == "courses") {
        result = state_.listCourses();
      } else if (method == "POST" && parts.size() == 3 && parts[0] == "courses" && parts[2] == "lessons") {
        result = state_.addLesson(parts[1], parseBody(request.stream()));
        status = Poco::Net::HTTPResponse::HTTP_CREATED;
      } else if (method == "GET" && parts.size() == 3 && parts[0] == "courses" && parts[2] == "lessons") {
        result = state_.listLessons(parts[1]);
      } else if (method == "POST" && parts.size() == 3 && parts[0] == "courses" && parts[2] == "enrollments") {
        result = state_.enrollUser(parts[1], parseBody(request.stream()));
        status = Poco::Net::HTTPResponse::HTTP_CREATED;
      } else if (method == "GET" && parts.size() == 3 && parts[0] == "users" && parts[2] == "courses") {
        result = state_.listUserCourses(parts[1]);
      } else if (method == "POST" && parts.size() == 5 && parts[0] == "users" && parts[2] == "lessons" && parts[4] == "completion") {
        result = state_.completeLesson(parts[1], parts[3]);
        status = Poco::Net::HTTPResponse::HTTP_CREATED;
      } else {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
        result.set("error", "endpoint not found");
        send(response, result);
        return;
      }

      response.setStatus(static_cast<Poco::Net::HTTPResponse::HTTPStatus>(status));
      send(response, result);
    } catch (const std::exception& error) {
      response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
      Poco::JSON::Object result;
      result.set("error", error.what());
      send(response, result);
    }
  }

private:
  LmsState& state_;

  void send(Poco::Net::HTTPServerResponse& response, const Poco::JSON::Object& body) {
    response.setContentType("application/json; charset=utf-8");
    std::ostream& output = response.send();
    body.stringify(output, 2);
  }
};

class LmsHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
public:
  explicit LmsHandlerFactory(LmsState& state) : state_(state) {
  }

  Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
    return new LmsHandler(state_);
  }

private:
  LmsState& state_;
};

volatile std::sig_atomic_t stopRequested = 0;

void stopServer(int) {
  stopRequested = 1;
}

int main() {
  try {
    RabbitMqClient rabbit(
        envOrDefault("RABBITMQ_HOST", "127.0.0.1"),
        envIntOrDefault("RABBITMQ_PORT", 5672),
        envOrDefault("RABBITMQ_USER", "lms"),
        envOrDefault("RABBITMQ_PASSWORD", "lms"));

    rabbit.waitUntilReady();
    rabbit.declareTopology();

    LmsState state(rabbit);
    const int port = envIntOrDefault("HTTP_PORT", 8080);
    Poco::Net::ServerSocket socket(port);
    Poco::Net::HTTPServerParams::Ptr params = new Poco::Net::HTTPServerParams();
    Poco::Net::HTTPServer server(new LmsHandlerFactory(state), socket, params);

    std::signal(SIGINT, stopServer);
    std::signal(SIGTERM, stopServer);

    server.start();
    std::cout << "producer HTTP API started on port " << port << std::endl;
    while (!stopRequested) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    server.stop();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "producer error: " << error.what() << '\n';
    return 1;
  }
}
