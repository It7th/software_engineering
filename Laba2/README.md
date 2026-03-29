# Лаба 2: мини-LMS на POCO

Небольшой REST API для варианта 19: система управления обучением.
В проекте есть три основные сущности:

- пользователь;
- курс;
- урок.

Сервис позволяет зарегистрировать пользователя, искать его по логину или по маске имени/фамилии, создавать курсы, добавлять в них уроки, записывать студента на курс и смотреть список его курсов. Для удобства я оставил ещё пару служебных вещей: `GET /health`, публикацию курса через `PATCH` и отметку о прохождении урока.

## Что внутри

- `C++17`
- `POCO` для HTTP-сервера и JSON
- `SQLite` как простое хранилище
- `openapi.yaml` со спецификацией
- `Dockerfile` и `docker-compose.yaml`
- один smoke-тест на основной сценарий

Аутентификация сделана без JWT: после логина сервис создаёт обычный session token, который потом передаётся в заголовке `Authorization: Bearer <token>`.

Роли такие:

- `ADMIN`
- `INSTRUCTOR`
- `STUDENT`

При первом запуске автоматически создаются два пользователя:

- `admin / admin123`
- `instructor / instructor123`

Самостоятельная регистрация через `POST /auth/register` всегда создаёт `STUDENT`.

## Структура проекта

```text
Laba2/
├── CMakeLists.txt
├── Dockerfile
├── Makefile
├── README.md
├── docker-compose.yaml
├── openapi.yaml
├── src/
│   ├── api_handler.cpp
│   ├── application.cpp
│   ├── lms.hpp
│   ├── lms_core.cpp
│   └── main.cpp
└── tests/
    └── test_api.py
```


## Запуск через Docker

Самый простой вариант:

```bash
docker compose up --build
```

После старта:

- API: `http://localhost:8080`
- Swagger UI: `http://localhost:8081`

База хранится в volume `lms-api-data`, поэтому данные не пропадают после перезапуска контейнера.

## Локальная сборка

Если POCO и SQLite уже стоят в системе, можно собрать локально.

### Через Makefile

```bash
make
PORT=8080 DB_PATH=data/lms.db ./bin/lms_api
```

Если библиотеки стоят не в стандартном месте, можно передать пути:

```bash
make POCO_PREFIX=/opt/homebrew SQLITE_PREFIX=/opt/homebrew/opt/sqlite
```

### Через CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
PORT=8080 DB_PATH=data/lms.db ./build/lms_api
```

## Короткий сценарий работы

### 1. Логин преподавателя

```bash
curl -X POST http://localhost:8080/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "login":"instructor",
    "password":"instructor123"
  }'
```

### 2. Создание курса

```bash
curl -X POST http://localhost:8080/courses \
  -H "Authorization: Bearer <INSTRUCTOR_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{
    "title":"HTTP Basics",
    "description":"Короткий курс по REST и HTTP"
  }'
```

### 3. Добавление урока

```bash
curl -X POST http://localhost:8080/courses/1/lessons \
  -H "Authorization: Bearer <INSTRUCTOR_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{
    "title":"GET и POST",
    "description":"Разница между чтением и изменением данных",
    "position":1
  }'
```

### 4. Публикация курса

```bash
curl -X PATCH http://localhost:8080/courses/1 \
  -H "Authorization: Bearer <INSTRUCTOR_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{
    "status":"PUBLISHED"
  }'
```

### 5. Регистрация студента и запись на курс

```bash
curl -X POST http://localhost:8080/auth/register \
  -H "Content-Type: application/json" \
  -d '{
    "login":"student01",
    "firstName":"Ivan",
    "lastName":"Petrov",
    "email":"student01@example.com",
    "password":"secret123"
  }'
```

```bash
curl -X POST http://localhost:8080/courses/1/enrollments \
  -H "Authorization: Bearer <STUDENT_TOKEN>"
```

### 6. Просмотр курсов пользователя

```bash
curl -X GET http://localhost:8080/users/3/courses \
  -H "Authorization: Bearer <STUDENT_TOKEN>"
```

## Что проверить руками

Если хочется быстро потыкать API без тестов:

- регистрация нового студента;
- логин под `instructor`;
- создание курса и пары уроков;
- публикация курса;
- запись студента на курс;
- попытка второй раз записаться на тот же курс, чтобы увидеть `409`.

## Тест

В `tests/test_api.py` лежит простой smoke-тест. Он не претендует на полноценный unit/integration набор, но проверяет, что основной поток не развалился:

- логин системных пользователей;
- регистрация студента;
- создание и публикация курса;
- добавление уроков;
- запись на курс;
- просмотр списка курсов пользователя;
- несколько негативных случаев.

Запуск:

```bash
python3 tests/test_api.py
```

Если API поднят не на `localhost:8080`, можно переопределить адрес:

```bash
BASE_URL=http://localhost:8080 python3 tests/test_api.py
```

## Замечания

- Валидация здесь базовая: обязательные поля проверяются, но без сложных правил
- Всё приложение остаётся одним сервисом, без разделения на несколько микросервисов
- `Swagger UI` запускается отдельным контейнером и просто читает локальный `openapi.yaml`
