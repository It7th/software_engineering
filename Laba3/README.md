# Домашнее задание 03: Проектирование и оптимизация реляционной БД

Вариант 19: **система управления обучением (LMS)**.  
Референс домена: [Moodle](https://www.moodle.org/).


## Что внутри

- `schema.sql` — схема PostgreSQL, ограничения и индексы.
- `data.sql` — тестовые данные, минимум 10 записей в каждой таблице.
- `queries.sql` — SQL для основных операций варианта.
- `optimization.md` — объяснение индексов и запросов для `EXPLAIN`.
- `Dockerfile`, `docker-compose.yaml` — запуск API и PostgreSQL.
- `src/` — C++ API на POCO, подключённый к PostgreSQL через `libpqxx`.
- `tests/test_api.py` — smoke-тест основного сценария.

## Архитектурная идея схемы


- `identity` — пользователи.
- `auth` — сессии.
- `catalog` — курсы и уроки.
- `learning` — записи на курсы и прохождение уроков.

- `identity` соответствует `User Service`;
- `catalog` соответствует `Course Service`;
- `learning` соответствует `Learning Service`.


## Примитивы системы

Базовые примитивы модели:

- `User`
- `Course`
- `Lesson`
- `Enrollment`
- `LessonCompletion`
- `Session`


## Схема БД

### `identity.users`

Хранит пользователей системы.

Ключевые поля:

- `id`
- `login`
- `first_name`
- `last_name`
- `email`
- `password_hash`
- `role`
- `status`
- `created_at`

Почему так:

- `role` и `status` оставлены строками с `CHECK`, а не PostgreSQL `ENUM`, чтобы модель было проще расширять и мигрировать;

### `auth.sessions`

Хранит session token, который API выдаёт после логина.

Ключевые поля:

- `id`
- `token`
- `user_id`
- `created_at`

### `catalog.courses`

Хранит курсы.

Ключевые поля:

- `id`
- `title`
- `description`
- `author_id`
- `status`
- `created_at`

### `catalog.lessons`

Хранит уроки внутри курса.

Ключевые поля:

- `id`
- `course_id`
- `title`
- `description`
- `position`
- `created_at`

Ключевое ограничение:

- `UNIQUE (course_id, position)` — внутри одного курса две темы не могут занимать одну позицию.

### `learning.enrollments`

Хранит запись пользователя на курс.

Ключевые поля:

- `id`
- `user_id`
- `course_id`
- `status`
- `enrolled_at`

Ключевое ограничение:

- `UNIQUE (user_id, course_id)` — повторная запись на тот же курс запрещена.

### `learning.lesson_completions`

Хранит факт прохождения урока.

Ключевые поля:

- `user_id`
- `lesson_id`
- `completed_at`

Здесь выбран **составной первичный ключ** `(user_id, lesson_id)`. Для этой таблицы отдельный surrogate id не даёт полезной семантики: запись существует только как факт “этот пользователь прошёл этот урок”.

## Ограничения

В схеме есть несколько типов ограничений:

`NOT NULL`
- на все обязательные доменные поля.

`UNIQUE`
- `identity.users.login`
- `identity.users.email`
- `auth.sessions.token`
- `catalog.lessons(course_id, position)`
- `learning.enrollments(user_id, course_id)`

`CHECK`
- допустимые значения ролей;
- допустимые значения статусов пользователя, курса и enrollment;
- `position > 0`;
- защита от пустых строк в названиях, логине, email и описаниях.

`FOREIGN KEY`
- курс ссылается на автора;
- урок ссылается на курс;
- запись на курс ссылается на пользователя и курс;
- прохождение урока ссылается на пользователя и урок;
- сессия ссылается на пользователя.

## Индексы

### Автоматические

- все первичные ключи;
- все `UNIQUE` ограничения.

### Индексы, добавленные вручную

`idx_sessions_user_id`
- нужен для запросов по пользователю и убирает лишнюю нагрузку с FK `auth.sessions.user_id`.

`idx_courses_author_id`
- используется в проверках “этот курс принадлежит этому преподавателю?” и пригодится для выборок всех курсов автора.

`idx_courses_status_id`
- ускоряет публичный каталог `GET /courses`, где фильтрация почти всегда идёт по `status = 'PUBLISHED'`.

`idx_enrollments_course_id`
- полезен для FK `course_id` и сценариев, где стартовой точкой является курс, а не пользователь.

`idx_enrollments_user_id_id`
- добавлен под endpoint `GET /users/{userId}/courses`: запрос фильтрует по `user_id` и сразу же сортирует строки по `id`.

`idx_lesson_completions_lesson_id`
- нужен для FK `lesson_id`, каскадных операций и join-ов, где вход в данные идёт от урока.

`idx_users_first_name_trgm`, `idx_users_last_name_trgm`
- нужны именно под поиск по маске через `ILIKE '%mask%'`; обычный B-tree для такого запроса почти бесполезен.

Отдельный индекс только на `catalog.lessons(course_id)` я не добавлял: эту роль уже закрывает уникальный индекс `UNIQUE (course_id, position)`, потому что он начинается с того же ведущего столбца.

## Какие операции поддерживаются

Базовые операции варианта:

- создание пользователя;
- поиск пользователя по логину;
- поиск пользователя по маске имени и фамилии;
- создание курса;
- получение списка курсов;
- добавление урока в курс;
- получение уроков курса;
- запись пользователя на курс;
- получение курсов пользователя;
- отметка о прохождении урока.

Дополнительно из `Laba2` сохранены:

- `POST /auth/register`
- `POST /auth/login`
- `PATCH /courses/{courseId}`
- `GET /health`

## Запуск через Docker

Самый простой способ проверки:

```bash
docker compose up --build
```

После старта:

- API: `http://localhost:8080`
- Swagger UI: `http://localhost:8081`
- PostgreSQL: `localhost:5432`

Параметры БД:

- database: `lms_db`
- user: `lms`
- password: `lms`

Практический момент:

- `schema.sql` и `data.sql` выполняются только при первом создании volume PostgreSQL;
- если нужно заново переинициализировать БД, используйте:

```bash
docker compose down -v
docker compose up --build
```

## Локальная сборка

Для локальной сборки понадобятся:

- `Poco`
- `libpqxx`
- `pkg-config`
- `CMake` или `make`

`pkg-config` здесь обязателен: и `CMake`, и `Makefile` используют его для поиска `libpqxx`.
Отдельно нужен доступный PostgreSQL-инстанс: локальный или поднятый через Docker.

Сборка через CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
PORT=8080 DATABASE_URL=postgresql://lms:lms@127.0.0.1:5432/lms_db ./build/lms_api
```

Сборка через Makefile:

```bash
make
PORT=8080 DATABASE_URL=postgresql://lms:lms@127.0.0.1:5432/lms_db ./bin/lms_api
```

## Тестовые пользователи

В `data.sql` заранее загружены системные пользователи:

- `admin / admin123`
- `instructor / instructor123`

Для остальных тестовых студентов используется пароль `secret123`. Это удобно для ручной проверки login / enrollment / completion без дополнительной подготовки данных.

## Проверка API

После запуска через Docker я проверяю API так:

```bash
python3 tests/test_api.py
```

Если API слушает не `localhost:8080`, адрес можно переопределить:

```bash
BASE_URL=http://localhost:8080 python3 tests/test_api.py
```
