# Домашнее задание 01: Документирование архитектуры в Structurizr

## 1. Цель и выбранный вариант
- Курс: «Архитектура программных систем».
- Цель: описать архитектуру системы в подходе Architecture as Code с использованием C4 и Structurizr DSL.
- Вариант: **19. Система управления обучением**.
- Референс домена: [Moodle](https://www.moodle.org/).

## 2. Функциональные требования (API)
Система должна поддерживать API:
- `POST /users` — создание нового пользователя.
- `GET /users/by-login?login=...` — поиск пользователя по логину.
- `GET /users/search?firstNameMask=...&lastNameMask=...` — поиск пользователя по маске имени и фамилии.
- `POST /courses` — создание курса.
- `GET /courses` — получение списка курсов.
- `POST /courses/{courseId}/lessons` — добавление урока в курс.
- `GET /courses/{courseId}/lessons` — получение уроков курса.
- `POST /courses/{courseId}/enrollments` — запись пользователя на курс.
- `GET /users/{userId}/courses` — получение курсов пользователя.
- `POST /users/{userId}/lessons/{lessonId}/completion` — отметка о прохождении урока.

## 3. Роли пользователей
- **Студент**:
  - просматривает каталог курсов;
  - получает список уроков курса;
  - записывается на курс;
  - просматривает свои курсы;
  - отмечает прохождение урока.
- **Преподаватель**:
  - создает курс;
  - добавляет уроки в курс;
  - просматривает уроки курса.
- **Администратор**:
  - создает пользователей;
  - ищет пользователя по логину;
  - ищет пользователей по маске имени и фамилии.

## 4. Внешние системы
- **Email Notification Provider** — отправка email-уведомлений о записи на курс и завершении обучения.
- **Message Broker** — инфраструктурная внешняя система для передачи асинхронных доменных событий.

## 5. Use Case Matrix (роль -> действия)
| Роль | Use case | Основной endpoint |
|---|---|---|
| Администратор | Создать пользователя | `POST /users` |
| Администратор | Найти пользователя по логину | `GET /users/by-login` |
| Администратор | Найти пользователей по маске имени и фамилии | `GET /users/search` |
| Преподаватель | Создать курс | `POST /courses` |
| Преподаватель | Добавить урок в курс | `POST /courses/{courseId}/lessons` |
| Преподаватель | Получить уроки курса | `GET /courses/{courseId}/lessons` |
| Студент | Получить список курсов | `GET /courses` |
| Студент | Записаться на курс | `POST /courses/{courseId}/enrollments` |
| Студент | Получить свои курсы | `GET /users/{userId}/courses` |
| Студент | Отметить прохождение урока | `POST /users/{userId}/lessons/{lessonId}/completion` |

Примечание: публичный каталог возвращает только курсы в статусе `PUBLISHED`, а внутренние изменения прогресса и уведомления реализуются сервисами как внутренние операции без дополнительных обязательных публичных endpoint сверх ТЗ.

## 6. Контейнерная архитектура (C2)
| Контейнер | Технология | Назначение | Владелец use case |
|---|---|---|---|
| Web Application | React SPA | UI для студента, преподавателя и администратора | Вход в систему для всех пользовательских сценариев |
| API Gateway / BFF | Node.js REST API | Единая точка входа, аутентификация, маршрутизация | Оркестрация вызовов доменных сервисов |
| User Service | REST API | Пользователи и пользовательский поиск | `POST /users`, `GET /users/by-login`, `GET /users/search` |
| Course Service | REST API | Курсы, уроки и каталог курсов | `POST /courses`, `GET /courses`, `POST /courses/{courseId}/lessons`, `GET /courses/{courseId}/lessons` |
| Learning Service | REST API | Запись на курс, список курсов пользователя, прохождение уроков | `POST /courses/{courseId}/enrollments`, `GET /users/{userId}/courses`, `POST /users/{userId}/lessons/{lessonId}/completion` |
| Notification Worker | Async worker | Отправка email-уведомлений по событиям обучения | Обработка события `UserEnrolled` и `LessonCompleted` |
| Relational Database | PostgreSQL | Хранение доменных данных с логическим разделением по схемам `user`/`course`/`learning` | `users`, `courses`, `lessons`, `enrollments`, `lesson_completions` |

## 7. Взаимодействие контейнеров
- `Web Application -> API Gateway / BFF`: `HTTPS/JSON`.
- `API Gateway / BFF -> User/Course/Learning Services`: `HTTPS/REST`.
- `Learning Service -> User Service`: `HTTPS/REST`.
- `Learning Service -> Course Service`: `HTTPS/REST`.
- `User/Course/Learning Services -> PostgreSQL`: `SQL`.
- `Learning Service -> Message Broker`: `Async event publish`.
- `Notification Worker -> Message Broker`: `Async event consume`.
- `Notification Worker -> Email Notification Provider`: `HTTPS/REST`.

## 8. Основные сценарии взаимодействия
### 8.1 Создание пользователя
1. Администратор отправляет `POST /users` через Web Application.
2. API Gateway / BFF маршрутизирует запрос в User Service.
3. User Service валидирует данные, проверяет уникальность логина и сохраняет пользователя в PostgreSQL.
4. API Gateway / BFF возвращает результат в Web Application.

### 8.2 Создание курса и добавление урока
1. Преподаватель отправляет `POST /courses`.
2. API Gateway / BFF передает запрос в Course Service.
3. Course Service валидирует данные курса и сохраняет курс в PostgreSQL со статусом `DRAFT`.
4. Преподаватель отправляет `POST /courses/{courseId}/lessons`.
5. Course Service сохраняет урок в PostgreSQL и фиксирует позицию урока внутри курса.

### 8.3 Запись пользователя на курс
1. Студент отправляет `POST /courses/{courseId}/enrollments`.
2. Learning Service проверяет пользователя через User Service.
3. Learning Service проверяет курс и его статус через Course Service.
4. Learning Service сохраняет запись на курс со статусом `ACTIVE`.
5. Learning Service публикует событие `UserEnrolled` в Message Broker.
6. API Gateway / BFF возвращает подтверждение записи.
7. Notification Worker асинхронно отправляет email-уведомление студенту.

### 8.4 Отметка о прохождении урока
1. Студент отправляет `POST /users/{userId}/lessons/{lessonId}/completion`.
2. Learning Service проверяет, что студент записан на курс урока.
3. Learning Service сохраняет запись о прохождении в PostgreSQL.
4. При завершении последнего обязательного урока публикуется событие `LessonCompleted`.
5. Notification Worker может отправить итоговое письмо о завершении обучения.

## 9. Ключевой dynamic-сценарий
Выбран сценарий: **запись пользователя на курс**.

Последовательность:
1. Студент выбирает курс в Web Application.
2. Web Application отправляет `POST /courses/{courseId}/enrollments` в API Gateway / BFF.
3. API Gateway / BFF передает запрос в Learning Service.
4. Learning Service валидирует пользователя через User Service.
5. Learning Service проверяет курс и статус `PUBLISHED` через Course Service.
6. Learning Service сохраняет enrollment в PostgreSQL со статусом `ACTIVE`.
7. Learning Service публикует событие `UserEnrolled` в Message Broker.
8. API Gateway / BFF возвращает подтверждение в Web Application без ожидания уведомления.
9. Notification Worker потребляет событие `UserEnrolled`.
10. Notification Worker отправляет email через Email Notification Provider.

Примечание по надежности: события `UserEnrolled` и `LessonCompleted` публикуются из `Learning Service` через `transactional outbox`, а `Notification Worker` обрабатывает сообщения идемпотентно по `eventId`.

## 10. Принятые технологии и протоколы
- UI: `React SPA`.
- API edge: `Node.js REST API (BFF)`.
- Доменные сервисы: `REST API`.
- Хранилище: `PostgreSQL`.
- Асинхронный транспорт: `Message Broker (Kafka/RabbitMQ)`.
- Синхронные интеграции: `HTTPS/REST`.
- Доступ к БД: `SQL`.
- Асинхронное взаимодействие: `Publish/Subscribe`.

## 11. Модель данных и статусы
### Доменные типы
- `User { id, login, firstName, lastName, email, role, status, createdAt }`
- `Course { id, title, description, authorId, status, createdAt }`
- `Lesson { id, courseId, title, description, position, createdAt }`
- `Enrollment { id, userId, courseId, status, enrolledAt }`
- `LessonCompletion { userId, lessonId, completedAt }`

### Статусы и перечисления
- `User.role`: `ADMIN`, `INSTRUCTOR`, `STUDENT`.
- `User.status`: `ACTIVE`, `BLOCKED`.
- `Course.status`: `DRAFT`, `PUBLISHED`, `ARCHIVED`.
- `Enrollment.status`: `ACTIVE`, `COMPLETED`, `CANCELLED`.

## 12. Corner Cases и архитектурные правила
- `POST /users` должен обеспечивать уникальность `login`.
- `POST /courses/{courseId}/enrollments` не должен создавать повторную активную запись на один и тот же курс для одного пользователя.
- На курс можно записаться только если пользователь активен, а курс находится в статусе `PUBLISHED`.
- `POST /users/{userId}/lessons/{lessonId}/completion` не должен срабатывать для студента, который не записан на курс данного урока.
- Повторная отметка прохождения одного и того же урока должна быть идемпотентной и не должна создавать дубликаты.
- Позиция урока (`position`) должна быть уникальной внутри курса.
- Поиск по маске имени и фамилии реализуется как case-insensitive partial match.
- При недоступности Email Notification Provider событие остается в очереди повторных попыток с retry policy и DLQ.
- Публикация событий из `Learning Service` выполняется через `Transactional Outbox`; `Notification Worker` обрабатывает события идемпотентно.

## 13. Ограничения и допущения
- Аутентификация инкапсулирована в API Gateway / BFF.
- Отдельный `Auth Service` не выделяется в рамках данного задания.
- При общей `PostgreSQL` логическое владение данными разделено по схемам `user`, `course`, `learning`: сервис пишет только в свою схему, доступ к чужим данным выполняется через API.
- Каталог курсов для студентов включает только курсы в статусе `PUBLISHED`.
- Отметки о прохождении уроков хранятся отдельно от самих уроков, чтобы не смешивать контент курса и прогресс пользователя.
- Уведомления отправляются асинхронно через связку `Message Broker + Notification Worker`.
- Уровень детализации ограничен C1/C2/Dynamic, без C3 (components).

## 14. Критерии готовности и self-check
- Все API из задания сопоставлены контейнерам-владельцам.
- На C1 отображены три роли пользователей и внешние системы.
- На C2 у каждого контейнера есть технология и ответственность.
- На связях указаны протоколы и типы интеграций.
- Dynamic-диаграмма отражает реальный порядок шагов записи на курс.
- Термины, статусы и названия контейнеров согласованы между `readme.md` и `workspace.dsl`.
