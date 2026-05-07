# Домашнее задание 05: кеширование и rate limiting для LMS

## Что внутри

- `src/server.js` - REST API на Node.js, Express и MongoDB.
- `performance_design.md` - разбор hot paths, кеша, TTL, инвалидации и rate limiting.
- `validation.js` - коллекции, индексы и валидация пользователей.
- `data.js` - тестовые данные.
- `Dockerfile` и `docker-compose.yaml` - запуск API, MongoDB и Redis.

## Что кешируется

Кеш сделан через Redis. Если Redis недоступен локально, API временно использует кеш в памяти процесса, чтобы сервис не падал при разработке.

Кешируются такие endpoints:

- `GET /courses`
- `GET /courses/:courseId/lessons`
- `GET /users/:userId/courses`
- `GET /users/by-login?login=...`

В ответах есть заголовок `X-Cache`:

- `MISS` - данных не было в кеше, API сходил в MongoDB;
- `HIT` - ответ взят из кеша.

## Инвалидация кеша

Кеш сбрасывается после операций записи:

- `POST /courses` сбрасывает кеш списка курсов;
- `POST /courses/:courseId/lessons` сбрасывает кеш уроков курса;
- `POST /courses/:courseId/enrollments` сбрасывает кеш курсов пользователя;
- `POST /users/:userId/lessons/:lessonId/completion` сбрасывает кеш курсов пользователя, потому что меняется прогресс.

## Rate limiting

Для endpoint:

```text
POST /users/:userId/lessons/:lessonId/completion
```

сделан token bucket:

- лимит по умолчанию - 20 запросов в минуту с одного IP;
- при превышении возвращается `429 Too Many Requests`;
- добавляются заголовки `X-RateLimit-Limit`, `X-RateLimit-Remaining`, `X-RateLimit-Reset`, `Retry-After`.

Лимит можно поменять через переменные окружения:

- `COMPLETION_RATE_LIMIT`
- `COMPLETION_RATE_WINDOW_MS`

## Запуск

```bash
cd Laba5
docker compose up --build
```

После запуска:

- API: `http://localhost:8080`
- MongoDB: `localhost:27017`
- Redis: `localhost:6379`

Проверка здоровья:

```bash
curl -sS http://localhost:8080/health
```

## Примеры запросов

### Список курсов с кешем

Первый запрос обычно будет `MISS`, второй - `HIT`:

```bash
curl -i -sS http://localhost:8080/courses
curl -i -sS http://localhost:8080/courses
```

### Уроки курса

```bash
curl -i -sS http://localhost:8080/courses/661000000000000000000001/lessons
curl -i -sS http://localhost:8080/courses/661000000000000000000001/lessons
```

### Добавление урока и сброс кеша

```bash
curl -sS -X POST http://localhost:8080/courses/661000000000000000000001/lessons \
  -H 'Content-Type: application/json' \
  -d '{
    "title": "Кеширование в API",
    "description": "Зачем хранить горячие ответы рядом с приложением.",
    "position": 3,
    "durationMinutes": 25,
    "status": "PUBLISHED"
  }'
```

После этого кеш `GET /courses/661000000000000000000001/lessons` будет сброшен.

### Отметка прохождения урока

```bash
curl -i -sS -X POST \
  http://localhost:8080/users/660000000000000000000004/lessons/662000000000000000000002/completion
```

В ответе будут rate limit заголовки. Если дергать endpoint слишком часто, API вернет `429`.

## Основные endpoints

- `POST /users` - создание пользователя;
- `GET /users/by-login?login=petrov` - поиск пользователя по логину;
- `GET /users/search?firstNameMask=па&lastNameMask=пет` - поиск по маске имени и фамилии;
- `POST /courses` - создание курса;
- `GET /courses` - список курсов;
- `POST /courses/:courseId/lessons` - добавление урока;
- `GET /courses/:courseId/lessons` - уроки курса;
- `POST /courses/:courseId/enrollments` - запись пользователя на курс;
- `GET /users/:userId/courses` - курсы пользователя;
- `POST /users/:userId/lessons/:lessonId/completion` - отметка прохождения урока.

## Что получилось

Частые чтения теперь не каждый раз идут в MongoDB. Для списка курсов и уроков это особенно заметно, потому что такие страницы обычно открывают часто, а меняются они редко. Rate limiting защищает endpoint прогресса от случайного или намеренного спама запросами.

