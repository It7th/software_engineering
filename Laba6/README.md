# Домашнее задание 06: Event-Driven архитектура для LMS


## Что внутри

- `event_driven_design.md` - описание архитектуры, команд, событий, брокера и CQRS.
- `event_catalog.md` - каталог событий.
- `src/producer.cpp` - POCO HTTP API, который публикует события.
- `src/consumer.cpp` - читает события и обновляет `data/read_model.json`.
- `docker-compose.yml` - RabbitMQ, producer и consumer.
- `Dockerfile`, `Makefile`, `CMakeLists.txt` - сборка C++ кода.

## Запуск

```bash
cd Laba6
docker compose up --build
```

После запуска:

- RabbitMQ UI: `http://localhost:15672`
- HTTP API: `http://localhost:8096`
- логин: `lms`
- пароль: `lms`
- read model: `data/read_model.json`

`producer` теперь работает как HTTP-сервис. События публикуются после POST-команд.

Посмотреть read model:

```bash
cat data/read_model.json
```

## Локальная сборка

Если RabbitMQ уже поднят, можно собрать C++ код локально. Нужны `POCO` и `rabbitmq-c`.

```bash
make
```

Через CMake:

```bash
cmake -S . -B build
cmake --build build
```

Запуск consumer:

```bash
RABBITMQ_HOST=127.0.0.1 RABBITMQ_PORT=5672 RABBITMQ_USER=lms RABBITMQ_PASSWORD=lms ./bin/consumer
```

Запуск producer:

```bash
RABBITMQ_HOST=127.0.0.1 RABBITMQ_PORT=5672 RABBITMQ_USER=lms RABBITMQ_PASSWORD=lms HTTP_PORT=8080 ./bin/producer
```

## Какие события публикуются

- `UserCreated`
- `CourseCreated`
- `LessonAdded`
- `UserEnrolledInCourse`
- `LessonCompleted`

Routing keys:

- `identity.user.created`
- `catalog.course.created`
- `catalog.lesson.added`
- `learning.enrollment.created`
- `learning.lesson.completed`

Все события попадают в exchange `lms.events`. Очередь `lms.projections` подписана на `#`, поэтому consumer видит весь поток и строит read model.

## Пример проверки

Создать пользователя:

```bash
curl -sS -X POST http://localhost:8096/users \
  -H 'Content-Type: application/json' \
  -d '{"login":"student01","firstName":"Ivan","lastName":"Petrov","email":"student01@example.com"}'
```

Найти пользователя по login:

```bash
curl -sS 'http://localhost:8096/users/by-login?login=student01'
```

Создать курс:

```bash
curl -sS -X POST http://localhost:8096/courses \
  -H 'Content-Type: application/json' \
  -d '{"title":"Event Driven LMS","description":"Events and CQRS","authorId":"user-900","status":"PUBLISHED"}'
```

Добавить уроки:

```bash
curl -sS -X POST http://localhost:8096/courses/course-200/lessons \
  -H 'Content-Type: application/json' \
  -d '{"title":"Domain events","position":1,"status":"PUBLISHED"}'

curl -sS -X POST http://localhost:8096/courses/course-200/lessons \
  -H 'Content-Type: application/json' \
  -d '{"title":"Read model projections","position":2,"status":"PUBLISHED"}'
```

Записать пользователя на курс:

```bash
curl -sS -X POST http://localhost:8096/courses/course-200/enrollments \
  -H 'Content-Type: application/json' \
  -d '{"userId":"user-100"}'
```

Отметить урок пройденным:

```bash
curl -sS -X POST http://localhost:8096/users/user-100/lessons/lesson-300/completion
```

В логах producer должны быть строки вида:

```text
published UserCreated by identity.user.created
published CourseCreated by catalog.course.created
```

В логах consumer:

```text
processed UserCreated
processed CourseCreated
processed LessonAdded
```

В `data/read_model.json` появятся пользователь, курс, уроки, запись на курс и прогресс по уроку.

