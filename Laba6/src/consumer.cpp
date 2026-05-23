#include "events.hpp"
#include "rabbitmq_client.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct UserView {
  std::string id;
  std::string login;
  std::string firstName;
  std::string lastName;
  std::set<std::string> courseIds;
  std::set<std::string> completedLessonIds;
};

struct LessonView {
  std::string id;
  std::string courseId;
  std::string title;
  int position = 0;
};

struct CourseView {
  std::string id;
  std::string title;
  std::string authorId;
  std::set<std::string> enrolledUserIds;
  std::vector<LessonView> lessons;
};

class ReadModel {
public:
  void apply(const std::string& eventJson) {
    const std::string eventType = extractJsonString(eventJson, "eventType");

    if (eventType == "UserCreated") {
      const std::string userId = extractJsonString(eventJson, "userId");
      UserView& user = users_[userId];
      user.id = userId;
      user.login = extractJsonString(eventJson, "login");
      user.firstName = extractJsonString(eventJson, "firstName");
      user.lastName = extractJsonString(eventJson, "lastName");
      return;
    }

    if (eventType == "CourseCreated") {
      const std::string courseId = extractJsonString(eventJson, "courseId");
      CourseView& course = courses_[courseId];
      course.id = courseId;
      course.title = extractJsonString(eventJson, "title");
      course.authorId = extractJsonString(eventJson, "authorId");
      return;
    }

    if (eventType == "LessonAdded") {
      LessonView lesson;
      lesson.id = extractJsonString(eventJson, "lessonId");
      lesson.courseId = extractJsonString(eventJson, "courseId");
      lesson.title = extractJsonString(eventJson, "title");
      lesson.position = extractJsonInt(eventJson, "position");

      lessons_[lesson.id] = lesson;
      CourseView& course = courses_[lesson.courseId];
      course.id = lesson.courseId;

      auto existing = std::find_if(course.lessons.begin(), course.lessons.end(), [&](const LessonView& item) {
        return item.id == lesson.id;
      });
      if (existing == course.lessons.end()) {
        course.lessons.push_back(lesson);
      } else {
        *existing = lesson;
      }

      std::sort(course.lessons.begin(), course.lessons.end(), [](const LessonView& left, const LessonView& right) {
        return left.position < right.position;
      });
      return;
    }

    if (eventType == "UserEnrolledInCourse") {
      const std::string userId = extractJsonString(eventJson, "userId");
      const std::string courseId = extractJsonString(eventJson, "courseId");
      users_[userId].id = userId;
      users_[userId].courseIds.insert(courseId);
      courses_[courseId].id = courseId;
      courses_[courseId].enrolledUserIds.insert(userId);
      return;
    }

    if (eventType == "LessonCompleted") {
      const std::string userId = extractJsonString(eventJson, "userId");
      const std::string lessonId = extractJsonString(eventJson, "lessonId");
      users_[userId].id = userId;
      users_[userId].completedLessonIds.insert(lessonId);
      return;
    }
  }

  void save(const std::string& path) const {
    std::ofstream file(path);
    if (!file) {
      throw std::runtime_error("cannot write read model: " + path);
    }

    file << "{\n";
    file << "  \"generatedAt\": \"" << nowIso() << "\",\n";
    writeUsers(file);
    file << ",\n";
    writeCourses(file);
    file << "\n}\n";
  }

private:
  std::map<std::string, UserView> users_;
  std::map<std::string, CourseView> courses_;
  std::map<std::string, LessonView> lessons_;

  void writeUsers(std::ofstream& file) const {
    file << "  \"users\": [\n";
    bool first = true;
    for (const auto& [id, user] : users_) {
      if (!first) {
        file << ",\n";
      }
      first = false;

      file << "    {\n";
      file << "      \"id\": \"" << jsonEscape(id) << "\",\n";
      file << "      \"login\": \"" << jsonEscape(user.login) << "\",\n";
      file << "      \"firstName\": \"" << jsonEscape(user.firstName) << "\",\n";
      file << "      \"lastName\": \"" << jsonEscape(user.lastName) << "\",\n";
      writeStringSet(file, "courseIds", user.courseIds, 6);
      file << ",\n";
      writeStringSet(file, "completedLessonIds", user.completedLessonIds, 6);
      file << ",\n";
      writeProgress(file, user);
      file << "\n    }";
    }
    file << "\n  ]";
  }

  void writeCourses(std::ofstream& file) const {
    file << "  \"courses\": [\n";
    bool firstCourse = true;
    for (const auto& [id, course] : courses_) {
      if (!firstCourse) {
        file << ",\n";
      }
      firstCourse = false;

      file << "    {\n";
      file << "      \"id\": \"" << jsonEscape(id) << "\",\n";
      file << "      \"title\": \"" << jsonEscape(course.title) << "\",\n";
      file << "      \"authorId\": \"" << jsonEscape(course.authorId) << "\",\n";
      file << "      \"lessonCount\": " << course.lessons.size() << ",\n";
      writeStringSet(file, "enrolledUserIds", course.enrolledUserIds, 6);
      file << ",\n";
      file << "      \"lessons\": [";
      for (size_t i = 0; i < course.lessons.size(); ++i) {
        const LessonView& lesson = course.lessons[i];
        if (i > 0) {
          file << ",";
        }
        file << "\n        {\"id\": \"" << jsonEscape(lesson.id) << "\", \"title\": \"" << jsonEscape(lesson.title)
             << "\", \"position\": " << lesson.position << "}";
      }
      if (!course.lessons.empty()) {
        file << "\n      ";
      }
      file << "]\n";
      file << "    }";
    }
    file << "\n  ]";
  }

  void writeStringSet(std::ofstream& file, const std::string& name, const std::set<std::string>& values, int indent) const {
    file << std::string(indent, ' ') << "\"" << name << "\": [";
    bool first = true;
    for (const std::string& value : values) {
      if (!first) {
        file << ", ";
      }
      first = false;
      file << "\"" << jsonEscape(value) << "\"";
    }
    file << "]";
  }

  void writeProgress(std::ofstream& file, const UserView& user) const {
    file << "      \"progress\": [";
    bool firstCourse = true;
    for (const std::string& courseId : user.courseIds) {
      const auto course = courses_.find(courseId);
      if (course == courses_.end()) {
        continue;
      }

      int completed = 0;
      for (const LessonView& lesson : course->second.lessons) {
        if (user.completedLessonIds.count(lesson.id) > 0) {
          ++completed;
        }
      }

      const int total = static_cast<int>(course->second.lessons.size());
      const int percent = total == 0 ? 0 : completed * 100 / total;

      if (!firstCourse) {
        file << ",";
      }
      firstCourse = false;
      file << "\n        {\"courseId\": \"" << jsonEscape(courseId) << "\", \"completedLessons\": " << completed
           << ", \"totalLessons\": " << total << ", \"percent\": " << percent << "}";
    }
    if (!firstCourse) {
      file << "\n      ";
    }
    file << "]";
  }
};

int main() {
  try {
    RabbitMqClient rabbit(
        envOrDefault("RABBITMQ_HOST", "127.0.0.1"),
        envIntOrDefault("RABBITMQ_PORT", 5672),
        envOrDefault("RABBITMQ_USER", "lms"),
        envOrDefault("RABBITMQ_PASSWORD", "lms"));

    rabbit.waitUntilReady();
    rabbit.declareTopology();

    const std::string readModelPath = envOrDefault("READ_MODEL_PATH", "data/read_model.json");
    const int maxMessages = envIntOrDefault("MAX_MESSAGES", 0);
    int processed = 0;

    ReadModel model;
    model.save(readModelPath);
    std::cout << "consumer started, read model: " << readModelPath << std::endl;

    while (maxMessages == 0 || processed < maxMessages) {
      const std::optional<std::string> message = rabbit.getOne();
      if (!message) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        continue;
      }

      model.apply(*message);
      model.save(readModelPath);
      ++processed;

      std::cout << "processed " << extractJsonString(*message, "eventType") << std::endl;
    }

    return 0;
  } catch (const std::exception& error) {
    std::cerr << "consumer error: " << error.what() << '\n';
    return 1;
  }
}
