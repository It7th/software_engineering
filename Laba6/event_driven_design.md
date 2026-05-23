# Проектирование Event-Driven архитектуры LMS


## 1. События и команды

Команда это намерение изменить систему. Событие это факт, который уже произошел.

| Команда | Событие | Кто должен узнать |
| --- | --- | --- |
| `CreateUser` | `UserCreated` | Search Projection, Notification Service, Audit Service |
| `CreateCourse` | `CourseCreated` | Course Catalog Projection, Search Projection, Audit Service |
| `AddLessonToCourse` | `LessonAdded` | Course Catalog Projection, Progress Projection, Notification Service |
| `EnrollUserInCourse` | `UserEnrolledInCourse` | User Courses Projection, Notification Service, Analytics Service |
| `MarkLessonCompleted` | `LessonCompleted` | Progress Projection, Analytics Service, Certificate Service |

Запросы событий не создают:

- поиск пользователя по login;
- поиск пользователя по имени и фамилии;
- получение списка курсов;
- получение уроков курса;
- получение курсов пользователя.

Они читают read model или основную базу, но не меняют состояние.

## 2. Компоненты системы

### Event producers

`Identity Service`

- принимает команду `CreateUser`;
- хранит пользователя;
- публикует `UserCreated`.

`Catalog Service`

- принимает `CreateCourse` и `AddLessonToCourse`;
- хранит курсы и уроки;
- публикует `CourseCreated` и `LessonAdded`.

`Learning Service`

- принимает `EnrollUserInCourse` и `MarkLessonCompleted`;
- хранит записи на курс и прохождение уроков;
- публикует `UserEnrolledInCourse` и `LessonCompleted`.

### Event consumers

`Course Catalog Projection`

- строит быстрый список курсов;
- обновляет состав уроков курса.

`User Courses Projection`

- строит экран "мои курсы";
- хранит статус записи пользователя на курс.

`Progress Projection`

- считает прогресс пользователя по курсу.

`Search Projection`

- обновляет данные для поиска пользователей и курсов.

`Notification Service`

- отправляет письма или push-уведомления после регистрации, записи на курс и новых уроков.

`Analytics Service`

- собирает статистику записей и прохождения уроков.

`Audit Service`

- хранит журнал важных действий.

## 3. Поток событий

Типовой сценарий:

1. Клиент вызывает `POST /users`.
2. `Identity Service` проверяет команду и сохраняет пользователя в write model.
3. После успешной записи публикуется `UserCreated`.
4. `Search Projection` обновляет индекс поиска, `Notification Service` отправляет приветствие, `Audit Service` пишет журнал.
5. Преподаватель вызывает `POST /courses`.
6. `Catalog Service` сохраняет курс и публикует `CourseCreated`.
7. Студент вызывает `POST /courses/{courseId}/enrollments`.
8. `Learning Service` сохраняет enrollment и публикует `UserEnrolledInCourse`.
9. `User Courses Projection` обновляет read model для `GET /users/{userId}/courses`.
10. Студент проходит урок, `Learning Service` публикует `LessonCompleted`.
11. `Progress Projection` пересчитывает прогресс курса.

Такой поток не заставляет основной API ждать уведомления, аналитику и пересчет витрин. Основная запись завершается быстро, остальные сервисы догоняют состояние через события.

## 4. RabbitMQ

Для этой работы выбран RabbitMQ.

Почему RabbitMQ:

- хорошо подходит для командно-событийных систем;
- удобно маршрутизирует события через `topic exchange`;
- легко запускается в Docker;
- для лабораторной проще Kafka, потому что не нужна история событий на месяцы назад.

### Exchange

```text
lms.events
type: topic
durable: true
```

Routing key строится по доменной зоне:

```text
identity.user.created
catalog.course.created
catalog.lesson.added
learning.enrollment.created
learning.lesson.completed
```

### Очереди

В реализации есть одна очередь:

```text
lms.projections
binding: #
```

Она получает все события и строит read model.

В полной схеме я бы разделил очереди так:

| Очередь | Binding | Назначение |
| --- | --- | --- |
| `lms.course_catalog_projection` | `catalog.#` | витрина каталога и уроков |
| `lms.user_courses_projection` | `learning.enrollment.*`, `learning.lesson.completed` | курсы пользователя и прогресс |
| `lms.search_projection` | `identity.user.created`, `catalog.course.created` | поиск |
| `lms.notifications` | `identity.user.created`, `learning.enrollment.created`, `catalog.lesson.added` | уведомления |
| `lms.analytics` | `learning.#`, `catalog.#` | аналитика |
| `lms.audit` | `#` | аудит |

## 5. Формат сообщения

Событие отправляется как JSON.

Общая обертка:

```json
{
  "eventId": "evt-...",
  "eventType": "LessonCompleted",
  "version": 1,
  "occurredAt": "2026-05-21T12:00:00Z",
  "source": "lms-api",
  "commandId": "cmd-...",
  "commandName": "MarkLessonCompleted",
  "correlationId": "demo-flow-001",
  "routingKey": "learning.lesson.completed",
  "payload": {
    "userId": "user-100",
    "courseId": "course-200",
    "lessonId": "lesson-300"
  }
}
```

`eventId` нужен для идемпотентности. Если consumer получил событие второй раз, он может понять, что уже обработал его.

`correlationId` связывает несколько событий одного пользовательского сценария.

`version` нужен для развития схемы события.

## 6. Гарантии доставки

Для LMS выбран режим `at-least-once`.

Настройки:

- exchange durable;
- queue durable;
- сообщения persistent;
- consumer обрабатывает событие идемпотентно;
- consumer подтверждает сообщение после успешной записи read model.

`Exactly-once` здесь не выбираю. В распределенной системе это дорого и часто превращается в `at-least-once` плюс идемпотентность на стороне consumer. Для LMS нормальнее пережить дубль `LessonCompleted`, чем усложнять брокер и хранилища.

Что делать с дублями:

- хранить обработанные `eventId`;
- использовать natural key, например `(userId, lessonId)` для прохождения урока;
- обновлять read model через upsert.

## 7. CQRS

CQRS здесь применим хорошо, потому что чтений больше, чем записей.

### Write side

Команды:

- `CreateUser`;
- `CreateCourse`;
- `AddLessonToCourse`;
- `EnrollUserInCourse`;
- `MarkLessonCompleted`.

Write model хранит нормализованные данные:

- `users`;
- `courses`;
- `lessons`;
- `enrollments`;
- `lesson_completions`.

Write side проверяет правила:

- login уникален;
- курс существует;
- позиция урока внутри курса уникальна;
- пользователь не записывается на один курс дважды;
- прохождение урока возможно только после записи на курс.

### Read side

Запросы:

- `FindUserByLogin`;
- `SearchUsersByName`;
- `GetCourses`;
- `GetCourseLessons`;
- `GetUserCourses`.

Read model можно хранить отдельно:

- `user_search_view`;
- `course_catalog_view`;
- `course_lessons_view`;
- `user_courses_view`;
- `course_progress_view`.

События синхронизируют read side:

- `UserCreated` обновляет `user_search_view`;
- `CourseCreated` обновляет `course_catalog_view`;
- `LessonAdded` обновляет `course_lessons_view`;
- `UserEnrolledInCourse` обновляет `user_courses_view`;
- `LessonCompleted` обновляет `course_progress_view`.

Из-за событий read model становится eventually consistent. Например, после отметки урока write side уже принял команду, а прогресс на экране может обновиться через небольшую задержку.

## 8. Что реализовано в коде

В `src/producer.cpp` реализован POCO HTTP API. Он принимает команды:

- `POST /users`;
- `POST /courses`;
- `POST /courses/{courseId}/lessons`;
- `POST /courses/{courseId}/enrollments`;
- `POST /users/{userId}/lessons/{lessonId}/completion`.

После успешной команды producer публикует соответствующее событие.

В `src/consumer.cpp` события читаются из очереди `lms.projections`. Consumer строит файл `data/read_model.json`.

Producer и consumer подключаются к RabbitMQ по AMQP через `rabbitmq-c`. HTTP API и JSON-сообщения сделаны через `POCO`.

В read model видно:

- пользователя;
- курс;
- уроки курса;
- список записанных пользователей;
- процент прогресса пользователя.
