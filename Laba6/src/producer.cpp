#include "events.hpp"
#include "rabbitmq_client.hpp"

#include <iostream>
#include <string>
#include <vector>

struct DemoEvent {
  std::string routingKey;
  std::string eventType;
  std::string commandName;
  std::string payload;
};

int main() {
  try {
    RabbitMqClient rabbit(
        envOrDefault("RABBITMQ_HOST", "127.0.0.1"),
        envIntOrDefault("RABBITMQ_PORT", 15672),
        envOrDefault("RABBITMQ_USER", "lms"),
        envOrDefault("RABBITMQ_PASSWORD", "lms"));

    rabbit.waitUntilReady();
    rabbit.declareTopology();

    const std::vector<DemoEvent> events = {
        {
            "identity.user.created",
            "UserCreated",
            "CreateUser",
            "{\"userId\":\"user-100\",\"login\":\"student01\",\"firstName\":\"Ivan\",\"lastName\":\"Petrov\",\"email\":\"student01@example.com\",\"role\":\"STUDENT\",\"status\":\"ACTIVE\"}"
        },
        {
            "catalog.course.created",
            "CourseCreated",
            "CreateCourse",
            "{\"courseId\":\"course-200\",\"title\":\"Event Driven LMS\",\"description\":\"Events and CQRS in a small learning system\",\"authorId\":\"user-900\",\"status\":\"PUBLISHED\"}"
        },
        {
            "catalog.lesson.added",
            "LessonAdded",
            "AddLessonToCourse",
            "{\"lessonId\":\"lesson-300\",\"courseId\":\"course-200\",\"title\":\"Domain events\",\"position\":1,\"status\":\"PUBLISHED\"}"
        },
        {
            "catalog.lesson.added",
            "LessonAdded",
            "AddLessonToCourse",
            "{\"lessonId\":\"lesson-301\",\"courseId\":\"course-200\",\"title\":\"Read model projections\",\"position\":2,\"status\":\"PUBLISHED\"}"
        },
        {
            "learning.enrollment.created",
            "UserEnrolledInCourse",
            "EnrollUserInCourse",
            "{\"enrollmentId\":\"enrollment-400\",\"userId\":\"user-100\",\"courseId\":\"course-200\",\"status\":\"ACTIVE\"}"
        },
        {
            "learning.lesson.completed",
            "LessonCompleted",
            "MarkLessonCompleted",
            "{\"userId\":\"user-100\",\"courseId\":\"course-200\",\"lessonId\":\"lesson-300\"}"
        }
    };

    for (const DemoEvent& event : events) {
      const std::string envelope = makeEvent(event.eventType, event.routingKey, event.commandName, event.payload);
      const bool routed = rabbit.publish(event.routingKey, envelope);
      std::cout << "published " << event.eventType << " by " << event.routingKey << " routed=" << (routed ? "true" : "false") << '\n';
    }

    return 0;
  } catch (const std::exception& error) {
    std::cerr << "producer error: " << error.what() << '\n';
    return 1;
  }
}
