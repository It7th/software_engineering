# Оптимизация запросов

В этой работе я не пытался “обвесить индексами всё подряд”. Сначала я посмотрел на реальные операции варианта 19 и на те SQL-запросы, которые действительно исполняет API из `Laba2`, а уже потом под них подбирал индексы и, где это было оправдано, переписывал сам запрос.

## Что именно оптимизировалось

Для LMS здесь критичны четыре сценария:

1. Поиск пользователя по маске имени и фамилии.
2. Публичный каталог опубликованных курсов.
3. Получение курсов конкретного пользователя.
4. Подсчёт прогресса по курсу при отметке о прохождении урока.

Два первых запроса хорошо реагируют на индексы и дают заметный выигрыш. Два последних уже изначально были не совсем “плохими”, поэтому там эффект скромнее: я оставил это в отчёте как есть, без приукрашивания.

## Подготовка данных для EXPLAIN

На исходных 10-20 строках планы мало что показывают. Поэтому для замеров в контейнер PostgreSQL были добавлены benchmark-данные:

- `identity.users`: +40 000 строк;
- `catalog.courses`: +30 000 строк;
- `learning.enrollments`: +8 000 строк для пользователя `user_id = 5`;
- отдельный курс `Progress Benchmark Course` с 5 000 уроками и 5 000 completion-записями.

Именно после этого уже снимались планы `EXPLAIN (ANALYZE, BUFFERS)`.

## Индексы и аргументация

`idx_courses_status_id`
- нужен под `GET /courses`, где фильтрация почти всегда идёт по `status = 'PUBLISHED'`.

`idx_courses_author_id`
- остаётся полезным для проверок владения курсом и сценариев “все курсы преподавателя”.

`idx_enrollments_course_id`
- индексирует FK `course_id` и помогает тем запросам, где точкой входа становится курс.

`idx_enrollments_user_id_id`
- добавлен под `GET /users/{userId}/courses`: фильтр по `user_id`, а затем выдача в порядке `e.id`.

`idx_lesson_completions_lesson_id`
- индекс для FK `lesson_id`, каскадных операций и join-ов, где движение начинается от урока.

`idx_sessions_user_id`
- делает дешёвыми запросы по сессиям пользователя и убирает лишнюю нагрузку с FK.

`idx_users_first_name_trgm`, `idx_users_last_name_trgm`
- нужны именно для `ILIKE '%mask%'`; обычный B-tree здесь почти не помогает.

Отдельный индекс на `catalog.lessons(course_id)` я сознательно не добавлял. Для этой роли уже хватает уникального индекса `UNIQUE (course_id, position)`: он начинается с того же ведущего столбца и покрывает типовой запрос “все уроки курса”.

## 1. Поиск пользователя по маске

Запрос:

```sql
SELECT id, login, first_name, last_name
FROM identity.users
WHERE first_name ILIKE '%special%'
  AND last_name ILIKE '%special%'
ORDER BY id;
```

До trigram-индексов:

```text
Sort (actual time=19.846..19.852 rows=100 loops=1)
  ->  Seq Scan on users (actual time=0.231..19.797 rows=100 loops=1)
        Rows Removed by Filter: 39911
Execution Time: 19.872 ms
```

После `idx_users_first_name_trgm` и `idx_users_last_name_trgm`:

```text
Sort (actual time=0.216..0.221 rows=100 loops=1)
  ->  Bitmap Heap Scan on users (actual time=0.037..0.198 rows=100 loops=1)
        ->  Bitmap Index Scan on idx_users_last_name_trgm
Execution Time: 0.255 ms
```

Вывод:

- здесь выигрыш самый заметный;
- полный `Seq Scan` исчезает;
- время падает примерно с `19.9 ms` до `0.26 ms`.

Это ожидаемо: поиск по `%mask%` без `pg_trgm` плохо масштабируется почти всегда.

## 2. Публичный каталог курсов

Запрос:

```sql
SELECT id, title, description, author_id, status
FROM catalog.courses
WHERE status = 'PUBLISHED'
ORDER BY id;
```

До индекса `(status, id)`:

```text
Sort (actual time=3.725..4.149 rows=10009 loops=1)
  ->  Seq Scan on courses (actual time=0.005..2.416 rows=10009 loops=1)
        Rows Removed by Filter: 20003
Execution Time: 4.506 ms
```

После `idx_courses_status_id`:

```text
Sort (actual time=3.114..3.535 rows=10009 loops=1)
  ->  Bitmap Heap Scan on courses (actual time=0.658..1.918 rows=10009 loops=1)
        ->  Bitmap Index Scan on idx_courses_status_id
Execution Time: 3.887 ms
```

Вывод:

- индекс даёт ожидаемый переход от полного сканирования к `Bitmap Index Scan`;
- выигрыш здесь не драматический, потому что опубликованных курсов много и сортировка по `id` всё равно остаётся;
- тем не менее запрос становится предсказуемее и меньше зависит от роста “непубличной” части каталога.

## 3. Курсы пользователя

Запрос:

```sql
SELECT
    e.id AS enrollment_id,
    c.id AS course_id,
    c.title,
    c.status,
    e.status AS enrollment_status
FROM learning.enrollments e
JOIN catalog.courses c ON c.id = e.course_id
WHERE e.user_id = 5
ORDER BY e.id;
```

До `idx_enrollments_user_id_id`:

```text
Sort (actual time=9.141..9.501 rows=8004 loops=1)
  ->  Nested Loop (actual time=0.210..8.051 rows=8004 loops=1)
        ->  Bitmap Heap Scan on enrollments e
              ->  Bitmap Index Scan on uq_enrollments_user_course
Execution Time: 9.857 ms
```

После `idx_enrollments_user_id_id`:

```text
Sort (actual time=8.801..9.188 rows=8004 loops=1)
  ->  Nested Loop (actual time=0.301..7.733 rows=8004 loops=1)
        ->  Bitmap Heap Scan on enrollments e
              ->  Bitmap Index Scan on idx_enrollments_user_id_id
Execution Time: 9.522 ms
```

Что здесь важно:

- ускорение есть, но оно умеренное;
- после фильтрации по `user_id` узким местом становится уже не чтение `enrollments`, а повторный join в `catalog.courses`;
- я сознательно не оставлял “экзотический” covering-index, потому что он только усложнял схему, а выигрыш на практике не окупал это усложнение.

Итог: индекс стоит своих нескольких строк DDL, но это уже не случай “получили x50”. И это нормальная инженерная картина.

## 4. Подсчёт прогресса по курсу

Здесь оптимизация была не столько индексной, сколько запросной. В коде я оставил более локальную формулировку: считать completion не “из всей таблицы completions пользователя”, а отталкиваться от уроков конкретного курса.

До переписывания запрос выглядел так:

```sql
SELECT COUNT(*)
FROM learning.lesson_completions lc
JOIN catalog.lessons l ON l.id = lc.lesson_id
WHERE lc.user_id = $1
  AND l.course_id = $2;
```

План:

```text
Aggregate (actual time=2.418..2.420 rows=1 loops=1)
  ->  Hash Join (actual time=1.043..2.142 rows=5000 loops=1)
        ->  Seq Scan on lessons l
        ->  Seq Scan on lesson_completions lc
Execution Time: 2.495 ms
```

После переписывания запрос стал таким:

```sql
SELECT COUNT(*)
FROM catalog.lessons l
JOIN learning.lesson_completions lc
  ON lc.lesson_id = l.id
 AND lc.user_id = $1
WHERE l.course_id = $2;
```

План:

```text
Aggregate (actual time=2.301..2.302 rows=1 loops=1)
  ->  Hash Join (actual time=1.047..2.070 rows=5000 loops=1)
        ->  Seq Scan on lessons l
        ->  Seq Scan on lesson_completions lc
Execution Time: 2.369 ms
```

Вывод:

- ускорение небольшое: примерно `2.50 ms -> 2.37 ms`;
- это не баг отчёта, а нормальный результат: у запроса и до этого были хорошие опорные ключи, поэтому чудесного скачка ждать неоткуда;
- переписывание я всё равно оставил, потому что новая форма лучше выражает смысл операции: “посчитать завершённые уроки именно в этом курсе”.

Для меня это важный момент: не каждая оптимизация обязана давать большой выигрыш, чтобы быть разумной.

## Итоговые наблюдения

После замеров получилось три практических вывода:

1. `pg_trgm` для масочного поиска действительно нужен, иначе endpoint поиска пользователей быстро деградирует.
2. Индекс по статусу курсов полезен, но его эффект зависит от доли опубликованных курсов в каталоге.
3. Для части LMS-запросов главная оптимизация — не “ещё один индекс”, а аккуратный выбор точки входа в данные и честная оценка, где уже начинает доминировать join.

В сумме это дало схему, которая:

- сохраняет доменные ограничения внутри самой БД;
- поддерживает реальные API-сценарии из `Laba2`;
- остаётся понятной и защищаемой без искусственно раздутой “магии оптимизации”.
