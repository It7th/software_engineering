# Домашнее задание 04: MongoDB для LMS

## Файлы работы

- `schema_design.md` — описание документной модели и объяснение embedded/reference.
- `validation.js` — создание коллекций, индексов и `$jsonSchema`-валидации для `users`.
- `data.js` — тестовые данные: по 10 документов в `users`, `courses`, `lessons`, `enrollments`.
- `queries.js` — CRUD-запросы, операторы `$eq`, `$ne`, `$gt`, `$lt`, `$in`, `$and`, `$or`, `$push`, `$pull`, `$addToSet` и aggregation pipeline.
- `src/server.js` — небольшой API на Node.js + MongoDB.
- `Dockerfile`, `docker-compose.yml` — запуск API и MongoDB.

## Запуск

```bash
cd Laba4
docker compose up --build
```

MongoDB будет доступна на `localhost:27017`, API — на `http://localhost:8080`.

При первом старте Mongo выполняет:

1. `validation.js`
2. `data.js`

Если нужно заново прогнать init-скрипты, проще удалить volume:

```bash
docker compose down -v
docker compose up --build
```

## Проверка базы руками

```bash
docker compose exec mongo mongosh lms_mongo
```

Несколько быстрых команд:

```js
db.users.countDocuments()
db.courses.find({}, { title: 1, status: 1 }).sort({ createdAt: -1 })
db.lessons.find({ courseId: ObjectId("661000000000000000000001") }).sort({ position: 1 })
db.enrollments.find({ userId: ObjectId("660000000000000000000004") })
```

Файл с запросами можно выполнить так:

```bash
docker compose exec -T mongo mongosh lms_mongo < queries.js
```

`queries.js` можно запускать кусками, потому что там есть insert/update/delete-примеры, которые меняют данные

## Проверка валидации

В `validation.js` есть намеренная попытка вставить невалидного пользователя. MongoDB должна ее отклонить: логин слишком короткий, email неверный, роль не входит в enum, пароль слишком короткий.

Отдельно можно проверить так:

```js
db.users.insertOne({
  login: "1",
  name: { first: "A", last: "B" },
  email: "wrong",
  passwordHash: "short",
  role: "GUEST",
  status: "ACTIVE",
  createdAt: new Date()
})
```

Ожидаемый результат — ошибка `Document failed validation`.

## API

### Создание пользователя

```bash
curl -sS -X POST http://localhost:8080/users \
  -H 'Content-Type: application/json' \
  -d '{
    "login": "denis.api",
    "firstName": "Денис",
    "lastName": "Ильин",
    "email": "denis.api@example.com",
    "password": "secret123",
    "role": "STUDENT"
  }'
```

### Поиск пользователя по логину

```bash
curl -sS 'http://localhost:8080/users/by-login?login=petrov'
```

### Поиск пользователя по маске имени и фамилии

```bash
curl -sS 'http://localhost:8080/users/search?firstNameMask=па&lastNameMask=пет'
```

### Создание курса

```bash
curl -sS -X POST http://localhost:8080/courses \
  -H 'Content-Type: application/json' \
  -d '{
    "title": "Практика MongoDB",
    "description": "Короткий курс с примерами запросов.",
    "authorId": "660000000000000000000002",
    "status": "DRAFT",
    "tags": ["mongodb", "practice"]
  }'
```

### Получение списка курсов

```bash
curl -sS http://localhost:8080/courses
curl -sS 'http://localhost:8080/courses?status=PUBLISHED'
```

### Добавление урока в курс

```bash
curl -sS -X POST http://localhost:8080/courses/661000000000000000000001/lessons \
  -H 'Content-Type: application/json' \
  -d '{
    "title": "Новый урок",
    "description": "Дополнительный материал по курсу.",
    "position": 3,
    "durationMinutes": 25,
    "status": "PUBLISHED"
  }'
```

### Получение уроков курса

```bash
curl -sS http://localhost:8080/courses/661000000000000000000001/lessons
```

### Запись пользователя на курс

```bash
curl -sS -X POST http://localhost:8080/courses/661000000000000000000006/enrollments \
  -H 'Content-Type: application/json' \
  -d '{ "userId": "660000000000000000000004" }'
```

### Получение курсов пользователя

```bash
curl -sS http://localhost:8080/users/660000000000000000000004/courses
```