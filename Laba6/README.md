# Домашнее задание 06: Event-Driven архитектура для LMS

Вариант 19: система управления обучением, похожая по домену на Moodle.

В этой работе я вынес основные изменения LMS в события. Команды меняют write model, после этого сервис публикует событие в RabbitMQ. Consumer читает события и собирает простую read model для экранов каталога, курсов пользователя и прогресса.

## Что внутри

- `event_driven_design.md` - описание архитектуры, команд, событий, брокера и CQRS.
- `event_catalog.md` - каталог событий.
- `src/producer.cpp` - публикует тестовый сценарий LMS.
- `src/consumer.cpp` - читает события и обновляет `data/read_model.json`.
- `docker-compose.yml` - RabbitMQ, producer и consumer.
- `Dockerfile`, `Makefile` - сборка C++ кода.

Код написан на C++17. Для учебного стенда producer и consumer ходят в RabbitMQ через Management HTTP API. Это позволяет собрать пример без сторонних C++ библиотек. В самой архитектуре целевой вариант такой же: durable topic exchange, routing keys и отдельные очереди потребителей.

## Запуск

```bash
cd Laba6
docker compose up --build
```

После запуска:

- RabbitMQ UI: `http://localhost:15672`
- логин: `lms`
- пароль: `lms`
- read model: `data/read_model.json`

`producer` публикует события и завершается. `consumer` остается работать и ждет новые сообщения.

Если нужно отправить события еще раз:

```bash
docker compose run --rm producer
```

Посмотреть read model:

```bash
cat data/read_model.json
```

## Локальная сборка

Если RabbitMQ уже поднят, можно собрать C++ код локально:

```bash
make
```

Запуск consumer:

```bash
RABBITMQ_HOST=127.0.0.1 RABBITMQ_PORT=15672 RABBITMQ_USER=lms RABBITMQ_PASSWORD=lms ./bin/consumer
```

Запуск producer:

```bash
RABBITMQ_HOST=127.0.0.1 RABBITMQ_PORT=15672 RABBITMQ_USER=lms RABBITMQ_PASSWORD=lms ./bin/producer
```

## Какие события публикуются в демо

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

## Что проверить

После `docker compose up --build` в логах producer должны быть строки вида:

```text
published UserCreated by identity.user.created routed=true
published CourseCreated by catalog.course.created routed=true
```

В логах consumer:

```text
processed UserCreated
processed CourseCreated
processed LessonAdded
```

В `data/read_model.json` появятся пользователь, курс, уроки, запись на курс и прогресс по уроку.

## Замечания

- Для продакшена я бы использовал нормальный AMQP client и manual ack после обработки сообщения.
- В учебном примере важнее показать exchange, routing, payload и CQRS-проекцию без тяжелых зависимостей.
- Read model пересобирается простым consumer. В реальной LMS такие проекции жили бы в отдельной базе.
