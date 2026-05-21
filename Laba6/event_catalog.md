# Каталог событий LMS

Все события публикуются в RabbitMQ exchange `lms.events`.

Формат общий для всех событий:

```json
{
  "eventId": "evt-...",
  "eventType": "UserCreated",
  "version": 1,
  "occurredAt": "2026-05-21T12:00:00Z",
  "source": "lms-api",
  "commandId": "cmd-...",
  "commandName": "CreateUser",
  "correlationId": "demo-flow-001",
  "routingKey": "identity.user.created",
  "payload": {}
}
```

Гарантия доставки для проектируемой системы: `at-least-once`.

## UserCreated

Routing key:

```text
identity.user.created
```

Когда возникает:

- после команды `CreateUser`.

Producer:

- `Identity Service`.

Consumers:

- `Search Projection`;
- `Notification Service`;
- `Audit Service`;
- `Analytics Service`.

Payload:

```json
{
  "userId": "user-100",
  "login": "student01",
  "firstName": "Ivan",
  "lastName": "Petrov",
  "email": "student01@example.com",
  "role": "STUDENT",
  "status": "ACTIVE"
}
```

Гарантия:

- `at-least-once`;
- consumer должен использовать `eventId` или `userId` для защиты от дублей.

## CourseCreated

Routing key:

```text
catalog.course.created
```

Когда возникает:

- после команды `CreateCourse`.

Producer:

- `Catalog Service`.

Consumers:

- `Course Catalog Projection`;
- `Search Projection`;
- `Audit Service`;
- `Analytics Service`.

Payload:

```json
{
  "courseId": "course-200",
  "title": "Event Driven LMS",
  "description": "Events and CQRS in a small learning system",
  "authorId": "user-900",
  "status": "PUBLISHED"
}
```

Гарантия:

- `at-least-once`;
- повторная обработка обновляет курс в read model через upsert.

## LessonAdded

Routing key:

```text
catalog.lesson.added
```

Когда возникает:

- после команды `AddLessonToCourse`.

Producer:

- `Catalog Service`.

Consumers:

- `Course Catalog Projection`;
- `Progress Projection`;
- `Notification Service`;
- `Audit Service`.

Payload:

```json
{
  "lessonId": "lesson-300",
  "courseId": "course-200",
  "title": "Domain events",
  "position": 1,
  "status": "PUBLISHED"
}
```

Гарантия:

- `at-least-once`;
- natural key для идемпотентности: `lessonId`.

## UserEnrolledInCourse

Routing key:

```text
learning.enrollment.created
```

Когда возникает:

- после команды `EnrollUserInCourse`.

Producer:

- `Learning Service`.

Consumers:

- `User Courses Projection`;
- `Progress Projection`;
- `Notification Service`;
- `Analytics Service`;
- `Audit Service`.

Payload:

```json
{
  "enrollmentId": "enrollment-400",
  "userId": "user-100",
  "courseId": "course-200",
  "status": "ACTIVE"
}
```

Гарантия:

- `at-least-once`;
- natural key для защиты от дублей: `(userId, courseId)`.

## LessonCompleted

Routing key:

```text
learning.lesson.completed
```

Когда возникает:

- после команды `MarkLessonCompleted`.

Producer:

- `Learning Service`.

Consumers:

- `Progress Projection`;
- `Analytics Service`;
- `Certificate Service`;
- `Notification Service`;
- `Audit Service`.

Payload:

```json
{
  "userId": "user-100",
  "courseId": "course-200",
  "lessonId": "lesson-300"
}
```

Гарантия:

- `at-least-once`;
- natural key для защиты от дублей: `(userId, lessonId)`.

## Сводная таблица

| Event | Routing key | Producer | Основные consumers |
| --- | --- | --- | --- |
| `UserCreated` | `identity.user.created` | Identity Service | Search, Notification, Audit |
| `CourseCreated` | `catalog.course.created` | Catalog Service | Catalog Projection, Search, Audit |
| `LessonAdded` | `catalog.lesson.added` | Catalog Service | Catalog Projection, Progress, Notification |
| `UserEnrolledInCourse` | `learning.enrollment.created` | Learning Service | User Courses, Progress, Notification |
| `LessonCompleted` | `learning.lesson.completed` | Learning Service | Progress, Analytics, Certificate |
