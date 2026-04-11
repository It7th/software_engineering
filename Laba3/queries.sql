-- Variant 19. SQL queries used by the LMS API.
-- Parameters use PostgreSQL positional placeholders: $1, $2, ...

-- 1. Create user
-- $1=login, $2=first_name, $3=last_name, $4=email, $5=password_hash, $6=role, $7=status
INSERT INTO identity.users (login, first_name, last_name, email, password_hash, role, status)
VALUES ($1, $2, $3, $4, $5, $6, $7)
RETURNING id, login, first_name, last_name, email, role, status, created_at;

-- 2. Find user by login
-- $1=login
SELECT id, login, first_name, last_name, email, role, status, created_at
FROM identity.users
WHERE login = $1
LIMIT 1;

-- 3. Search users by first/last name mask
-- $1=first_name_mask, $2=last_name_mask
SELECT id, login, first_name, last_name, email, role, status, created_at
FROM identity.users
WHERE first_name ILIKE ('%' || $1 || '%')
  AND last_name ILIKE ('%' || $2 || '%')
ORDER BY id;

-- 4. Create course
-- $1=title, $2=description, $3=author_id, $4=status
INSERT INTO catalog.courses (title, description, author_id, status)
VALUES ($1, $2, $3, $4)
RETURNING id, title, description, author_id, status, created_at;

-- 5a. Public course catalog
SELECT id, title, description, author_id, status, created_at
FROM catalog.courses
WHERE status = 'PUBLISHED'
ORDER BY id;

-- 5b. Internal course list for admin/instructor
SELECT id, title, description, author_id, status, created_at
FROM catalog.courses
ORDER BY id;

-- 6. Add lesson to course
-- $1=course_id, $2=title, $3=description, $4=position
INSERT INTO catalog.lessons (course_id, title, description, position)
VALUES ($1, $2, $3, $4)
RETURNING id, course_id, title, description, position, created_at;

-- 7. Get course lessons
-- $1=course_id
SELECT id, course_id, title, description, position, created_at
FROM catalog.lessons
WHERE course_id = $1
ORDER BY position;

-- 8. Enroll user into course
-- $1=user_id, $2=course_id
INSERT INTO learning.enrollments (user_id, course_id, status)
VALUES ($1, $2, 'ACTIVE')
RETURNING id, user_id, course_id, status, enrolled_at;

-- 9. Get courses for a user
-- $1=user_id
SELECT
    e.id AS enrollment_id,
    c.id AS course_id,
    c.title,
    c.description,
    c.status AS course_status,
    e.status AS enrollment_status,
    e.enrolled_at
FROM learning.enrollments e
JOIN catalog.courses c ON c.id = e.course_id
WHERE e.user_id = $1
ORDER BY e.id;

-- 10. Mark lesson as completed.
-- $1=user_id, $2=lesson_id
-- If is_enrolled = false, the API should return 409 and must not insert completion row.
BEGIN;

WITH lesson_context AS (
    SELECT l.id AS lesson_id, l.course_id
    FROM catalog.lessons l
    WHERE l.id = $2
),
enrollment_context AS (
    SELECT
        e.id AS enrollment_id,
        e.status AS enrollment_status,
        lc.lesson_id,
        lc.course_id
    FROM lesson_context lc
    JOIN learning.enrollments e
      ON e.course_id = lc.course_id
     AND e.user_id = $1
),
insert_completion AS (
    INSERT INTO learning.lesson_completions (user_id, lesson_id)
    SELECT $1, ec.lesson_id
    FROM enrollment_context ec
    ON CONFLICT (user_id, lesson_id) DO NOTHING
    RETURNING completed_at
),
course_progress AS (
    SELECT
        ec.course_id,
        (SELECT COUNT(*) FROM catalog.lessons l WHERE l.course_id = ec.course_id) AS lesson_count,
        (
            SELECT COUNT(*)
            FROM catalog.lessons l
            JOIN learning.lesson_completions c
              ON c.lesson_id = l.id
             AND c.user_id = $1
            WHERE l.course_id = ec.course_id
        ) AS completed_count
    FROM enrollment_context ec
),
update_enrollment AS (
    UPDATE learning.enrollments e
    SET status = 'COMPLETED'
    FROM enrollment_context ec
    JOIN course_progress cp ON cp.course_id = ec.course_id
    WHERE e.id = ec.enrollment_id
      AND cp.lesson_count > 0
      AND cp.completed_count >= cp.lesson_count
      AND e.status <> 'COMPLETED'
    RETURNING e.id
)
SELECT
    EXISTS (SELECT 1 FROM enrollment_context) AS is_enrolled,
    (
        NOT EXISTS (SELECT 1 FROM insert_completion)
        AND EXISTS (SELECT 1 FROM enrollment_context)
    ) AS already_completed,
    COALESCE(
        (SELECT completed_at FROM insert_completion LIMIT 1),
        (
            SELECT completed_at
            FROM learning.lesson_completions
            WHERE user_id = $1 AND lesson_id = $2
            LIMIT 1
        )
    ) AS completed_at,
    COALESCE(
        (SELECT 'COMPLETED' FROM update_enrollment LIMIT 1),
        (SELECT enrollment_status FROM enrollment_context LIMIT 1)
    ) AS enrollment_status;

COMMIT;

-- Additional service queries used by the API implementation

-- Login
-- $1=login, $2=password_hash
SELECT id, login, first_name, last_name, email, role, status, created_at
FROM identity.users
WHERE login = $1
  AND password_hash = $2
LIMIT 1;

-- Create session token
-- $1=token, $2=user_id
INSERT INTO auth.sessions (token, user_id)
VALUES ($1, $2)
RETURNING id, token, user_id, created_at;

-- Resolve auth context by token
-- $1=token
SELECT
    u.id, u.login, u.first_name, u.last_name, u.email, u.role, u.status, u.created_at
FROM auth.sessions s
JOIN identity.users u ON u.id = s.user_id
WHERE s.token = $1
LIMIT 1;
