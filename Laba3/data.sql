INSERT INTO identity.users (id, login, first_name, last_name, email, password_hash, role, status, created_at)
OVERRIDING SYSTEM VALUE
VALUES
    (1, 'admin', 'System', 'Administrator', 'admin@example.com', '240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9', 'ADMIN', 'ACTIVE', '2026-01-01T08:00:00Z'),
    (2, 'instructor', 'Default', 'Instructor', 'instructor@example.com', 'c1437a55f6e93b7049c4064af1b0920974e383a435283f5d0b0496ee4a8a47b5', 'INSTRUCTOR', 'ACTIVE', '2026-01-01T08:05:00Z'),
    (3, 'teacher_olga', 'Olga', 'Smirnova', 'olga.smirnova@example.com', 'cde383eee8ee7a4400adf7a15f716f179a2eb97646b37e089eb8d6d04e663416', 'INSTRUCTOR', 'ACTIVE', '2026-01-02T09:00:00Z'),
    (4, 'teacher_anna', 'Anna', 'Volkova', 'anna.volkova@example.com', 'cde383eee8ee7a4400adf7a15f716f179a2eb97646b37e089eb8d6d04e663416', 'INSTRUCTOR', 'ACTIVE', '2026-01-02T09:10:00Z'),
    (5, 'student_ivan', 'Ivan', 'Petrov', 'ivan.petrov@example.com', 'fcf730b6d95236ecd3c9fc2d92d7b6b2bb061514961aec041d6c7a7192f592e4', 'STUDENT', 'ACTIVE', '2026-01-03T10:00:00Z'),
    (6, 'student_maria', 'Maria', 'Ivanova', 'maria.ivanova@example.com', 'fcf730b6d95236ecd3c9fc2d92d7b6b2bb061514961aec041d6c7a7192f592e4', 'STUDENT', 'ACTIVE', '2026-01-03T10:05:00Z'),
    (7, 'student_pavel', 'Pavel', 'Sidorov', 'pavel.sidorov@example.com', 'fcf730b6d95236ecd3c9fc2d92d7b6b2bb061514961aec041d6c7a7192f592e4', 'STUDENT', 'ACTIVE', '2026-01-03T10:10:00Z'),
    (8, 'student_elena', 'Elena', 'Kozlova', 'elena.kozlova@example.com', 'fcf730b6d95236ecd3c9fc2d92d7b6b2bb061514961aec041d6c7a7192f592e4', 'STUDENT', 'ACTIVE', '2026-01-03T10:15:00Z'),
    (9, 'student_daria', 'Daria', 'Morozova', 'daria.morozova@example.com', 'fcf730b6d95236ecd3c9fc2d92d7b6b2bb061514961aec041d6c7a7192f592e4', 'STUDENT', 'BLOCKED', '2026-01-03T10:20:00Z'),
    (10, 'student_nikita', 'Nikita', 'Orlov', 'nikita.orlov@example.com', 'fcf730b6d95236ecd3c9fc2d92d7b6b2bb061514961aec041d6c7a7192f592e4', 'STUDENT', 'ACTIVE', '2026-01-03T10:25:00Z');

INSERT INTO auth.sessions (id, token, user_id, created_at)
OVERRIDING SYSTEM VALUE
VALUES
    (1, '00000000-0000-0000-0000-000000000001', 1, '2026-02-01T08:00:00Z'),
    (2, '00000000-0000-0000-0000-000000000002', 2, '2026-02-01T08:05:00Z'),
    (3, '00000000-0000-0000-0000-000000000003', 3, '2026-02-01T08:10:00Z'),
    (4, '00000000-0000-0000-0000-000000000004', 4, '2026-02-01T08:15:00Z'),
    (5, '00000000-0000-0000-0000-000000000005', 5, '2026-02-01T08:20:00Z'),
    (6, '00000000-0000-0000-0000-000000000006', 6, '2026-02-01T08:25:00Z'),
    (7, '00000000-0000-0000-0000-000000000007', 7, '2026-02-01T08:30:00Z'),
    (8, '00000000-0000-0000-0000-000000000008', 8, '2026-02-01T08:35:00Z'),
    (9, '00000000-0000-0000-0000-000000000009', 9, '2026-02-01T08:40:00Z'),
    (10, '00000000-0000-0000-0000-000000000010', 10, '2026-02-01T08:45:00Z');

INSERT INTO catalog.courses (id, title, description, author_id, status, created_at)
OVERRIDING SYSTEM VALUE
VALUES
    (1, 'HTTP Basics', 'Introduction to HTTP methods, headers and response codes.', 2, 'PUBLISHED', '2026-02-10T09:00:00Z'),
    (2, 'SQL Fundamentals', 'Relational design, joins and normalization basics.', 2, 'PUBLISHED', '2026-02-10T09:10:00Z'),
    (3, 'C++ for Backend', 'Memory model, RAII and service implementation patterns.', 3, 'DRAFT', '2026-02-11T10:00:00Z'),
    (4, 'Software Architecture', 'Modularity, interfaces and system decomposition.', 3, 'PUBLISHED', '2026-02-11T10:10:00Z'),
    (5, 'Linux Essentials', 'CLI, processes, filesystems and automation.', 4, 'PUBLISHED', '2026-02-12T11:00:00Z'),
    (6, 'Distributed Systems', 'Replication, consistency and fault tolerance.', 4, 'ARCHIVED', '2026-02-12T11:10:00Z'),
    (7, 'Web Security', 'Authentication, authorization and secure coding.', 2, 'PUBLISHED', '2026-02-13T12:00:00Z'),
    (8, 'PostgreSQL Performance', 'Indexes, EXPLAIN and query tuning.', 3, 'PUBLISHED', '2026-02-13T12:10:00Z'),
    (9, 'Testing APIs', 'Smoke tests, integration tests and contracts.', 4, 'DRAFT', '2026-02-14T13:00:00Z'),
    (10, 'Clean Code Workshop', 'Readable code, naming and maintainable refactoring.', 2, 'PUBLISHED', '2026-02-14T13:10:00Z');

INSERT INTO catalog.lessons (id, course_id, title, description, position, created_at)
OVERRIDING SYSTEM VALUE
VALUES
    (1, 1, 'HTTP Methods', 'GET, POST, PUT, PATCH and DELETE in practice.', 1, '2026-02-15T09:00:00Z'),
    (2, 1, 'Status Codes', '2xx, 4xx and 5xx families and common use cases.', 2, '2026-02-15T09:05:00Z'),
    (3, 2, 'Tables and Keys', 'Primary keys, foreign keys and relational thinking.', 1, '2026-02-15T09:10:00Z'),
    (4, 2, 'JOIN Patterns', 'INNER, LEFT and aggregation examples.', 2, '2026-02-15T09:15:00Z'),
    (5, 3, 'RAII Basics', 'Resource ownership and deterministic cleanup.', 1, '2026-02-15T09:20:00Z'),
    (6, 3, 'Service Composition', 'How to structure backend modules in C++.', 2, '2026-02-15T09:25:00Z'),
    (7, 4, 'Black Box Modules', 'Replacing components through stable interfaces.', 1, '2026-02-15T09:30:00Z'),
    (8, 4, 'Primitive-First Design', 'Choosing data primitives that reduce complexity.', 2, '2026-02-15T09:35:00Z'),
    (9, 5, 'Shell and Files', 'Working with files, pipes and shell utilities.', 1, '2026-02-15T09:40:00Z'),
    (10, 5, 'Processes and Automation', 'System processes and scheduled jobs.', 2, '2026-02-15T09:45:00Z'),
    (11, 6, 'Consistency Models', 'CAP, quorums and coordination tradeoffs.', 1, '2026-02-15T09:50:00Z'),
    (12, 6, 'Replication', 'Leader-follower and multi-primary patterns.', 2, '2026-02-15T09:55:00Z'),
    (13, 7, 'Authentication Basics', 'Sessions, tokens and credential storage.', 1, '2026-02-15T10:00:00Z'),
    (14, 7, 'Authorization Rules', 'Role checks and access boundaries.', 2, '2026-02-15T10:05:00Z'),
    (15, 8, 'Reading Execution Plans', 'Seq Scan, Index Scan and cost analysis.', 1, '2026-02-15T10:10:00Z'),
    (16, 8, 'Index Design', 'B-tree, GIN and covering access paths.', 2, '2026-02-15T10:15:00Z'),
    (17, 9, 'Smoke Testing', 'Quick end-to-end checks for the happy path.', 1, '2026-02-15T10:20:00Z'),
    (18, 9, 'Contract Testing', 'Stable API boundaries and schema checks.', 2, '2026-02-15T10:25:00Z'),
    (19, 10, 'Naming', 'Choosing names that explain intent.', 1, '2026-02-15T10:30:00Z'),
    (20, 10, 'Refactoring', 'Reducing complexity without changing behaviour.', 2, '2026-02-15T10:35:00Z');

INSERT INTO learning.enrollments (id, user_id, course_id, status, enrolled_at)
OVERRIDING SYSTEM VALUE
VALUES
    (1, 5, 1, 'ACTIVE', '2026-03-01T08:00:00Z'),
    (2, 5, 2, 'COMPLETED', '2026-03-01T08:10:00Z'),
    (3, 5, 4, 'ACTIVE', '2026-03-01T08:20:00Z'),
    (4, 6, 1, 'COMPLETED', '2026-03-01T08:30:00Z'),
    (5, 6, 5, 'ACTIVE', '2026-03-01T08:40:00Z'),
    (6, 7, 4, 'ACTIVE', '2026-03-01T08:50:00Z'),
    (7, 7, 7, 'ACTIVE', '2026-03-01T09:00:00Z'),
    (8, 8, 8, 'ACTIVE', '2026-03-01T09:10:00Z'),
    (9, 8, 10, 'ACTIVE', '2026-03-01T09:20:00Z'),
    (10, 9, 1, 'CANCELLED', '2026-03-01T09:30:00Z'),
    (11, 10, 5, 'COMPLETED', '2026-03-01T09:40:00Z'),
    (12, 10, 7, 'ACTIVE', '2026-03-01T09:50:00Z');

INSERT INTO learning.lesson_completions (user_id, lesson_id, completed_at)
VALUES
    (5, 1, '2026-03-05T10:00:00Z'),
    (5, 3, '2026-03-05T10:05:00Z'),
    (5, 4, '2026-03-05T10:10:00Z'),
    (5, 7, '2026-03-05T10:15:00Z'),
    (6, 1, '2026-03-05T10:20:00Z'),
    (6, 2, '2026-03-05T10:25:00Z'),
    (6, 9, '2026-03-05T10:30:00Z'),
    (7, 7, '2026-03-05T10:35:00Z'),
    (8, 15, '2026-03-05T10:40:00Z'),
    (10, 9, '2026-03-05T10:45:00Z'),
    (10, 10, '2026-03-05T10:50:00Z'),
    (10, 13, '2026-03-05T10:55:00Z');

SELECT setval(pg_get_serial_sequence('identity.users', 'id'), COALESCE((SELECT MAX(id) FROM identity.users), 1), true);
SELECT setval(pg_get_serial_sequence('auth.sessions', 'id'), COALESCE((SELECT MAX(id) FROM auth.sessions), 1), true);
SELECT setval(pg_get_serial_sequence('catalog.courses', 'id'), COALESCE((SELECT MAX(id) FROM catalog.courses), 1), true);
SELECT setval(pg_get_serial_sequence('catalog.lessons', 'id'), COALESCE((SELECT MAX(id) FROM catalog.lessons), 1), true);
SELECT setval(pg_get_serial_sequence('learning.enrollments', 'id'), COALESCE((SELECT MAX(id) FROM learning.enrollments), 1), true);
