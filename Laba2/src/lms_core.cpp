#include "lms.hpp"

#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/Statement.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DigestEngine.h>
#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>
#include <Poco/NumberParser.h>
#include <Poco/SHA2Engine.h>
#include <Poco/StreamCopier.h>
#include <Poco/Timestamp.h>
#include <Poco/UUIDGenerator.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <ostream>

using Poco::Data::Session;
using Poco::Data::Keywords::into;
using Poco::Data::Keywords::now;
using Poco::JSON::Object;
using Poco::JSON::Parser;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;

namespace lms
{

std::string trim(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();

    if (first >= last)
    {
        return "";
    }

    return std::string(first, last);
}

std::string toUpper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool isTruthy(std::string value)
{
    value = toLower(trim(value));
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

std::string nowIso()
{
    return Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::ISO8601_FORMAT, Poco::DateTimeFormatter::UTC);
}

std::string hashPassword(const std::string& password)
{
    Poco::SHA2Engine engine(Poco::SHA2Engine::SHA_256);
    engine.update(password);
    return Poco::DigestEngine::digestToHex(engine.digest());
}

std::string generateToken()
{
    return Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
}

int parseInt(const std::string& value, const std::string& fieldName)
{
    try
    {
        return Poco::NumberParser::parse(value);
    }
    catch (...)
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", fieldName + " must be an integer");
    }
}

std::map<std::string, std::string> queryParams(const Poco::URI& uri)
{
    std::map<std::string, std::string> result;
    for (const auto& [name, value] : uri.getQueryParameters())
    {
        result[name] = value;
    }
    return result;
}

std::string requireString(const Object::Ptr& body, const std::string& fieldName)
{
    if (!body || !body->has(fieldName))
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", fieldName + " is required");
    }

    const auto value = trim(body->get(fieldName).convert<std::string>());
    if (value.empty())
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", fieldName + " must not be empty");
    }

    return value;
}

std::optional<std::string> optionalString(const Object::Ptr& body, const std::string& fieldName)
{
    if (!body || !body->has(fieldName))
    {
        return std::nullopt;
    }

    return trim(body->get(fieldName).convert<std::string>());
}

int requireInt(const Object::Ptr& body, const std::string& fieldName)
{
    if (!body || !body->has(fieldName))
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", fieldName + " is required");
    }

    try
    {
        return body->get(fieldName).convert<int>();
    }
    catch (...)
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", fieldName + " must be an integer");
    }
}

std::string normalizeRole(const std::string& value)
{
    const auto normalized = toUpper(trim(value));
    if (normalized != ROLE_ADMIN && normalized != ROLE_INSTRUCTOR && normalized != ROLE_STUDENT)
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", "role must be ADMIN, INSTRUCTOR, or STUDENT");
    }
    return normalized;
}

std::string normalizeUserStatus(const std::string& value)
{
    const auto normalized = toUpper(trim(value));
    if (normalized != USER_STATUS_ACTIVE && normalized != USER_STATUS_BLOCKED)
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", "status must be ACTIVE or BLOCKED");
    }
    return normalized;
}

std::string normalizeCourseStatus(const std::string& value)
{
    const auto normalized = toUpper(trim(value));
    if (normalized != COURSE_STATUS_DRAFT && normalized != COURSE_STATUS_PUBLISHED && normalized != COURSE_STATUS_ARCHIVED)
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", "course status must be DRAFT, PUBLISHED, or ARCHIVED");
    }
    return normalized;
}

Object::Ptr userToJson(const UserRow& user)
{
    Object::Ptr payload = new Object();
    payload->set("id", user.id);
    payload->set("login", user.login);
    payload->set("firstName", user.firstName);
    payload->set("lastName", user.lastName);
    payload->set("email", user.email);
    payload->set("role", user.role);
    payload->set("status", user.status);
    payload->set("createdAt", user.createdAt);
    return payload;
}

Object::Ptr courseToJson(const CourseRow& course)
{
    Object::Ptr payload = new Object();
    payload->set("id", course.id);
    payload->set("title", course.title);
    payload->set("description", course.description);
    payload->set("authorId", course.authorId);
    payload->set("status", course.status);
    payload->set("createdAt", course.createdAt);
    return payload;
}

Object::Ptr lessonToJson(const LessonRow& lesson)
{
    Object::Ptr payload = new Object();
    payload->set("id", lesson.id);
    payload->set("courseId", lesson.courseId);
    payload->set("title", lesson.title);
    payload->set("description", lesson.description);
    payload->set("position", lesson.position);
    payload->set("createdAt", lesson.createdAt);
    return payload;
}

Object::Ptr enrollmentToJson(const EnrollmentRow& enrollment)
{
    Object::Ptr payload = new Object();
    payload->set("id", enrollment.id);
    payload->set("userId", enrollment.userId);
    payload->set("courseId", enrollment.courseId);
    payload->set("status", enrollment.status);
    payload->set("enrolledAt", enrollment.enrolledAt);
    return payload;
}

Object::Ptr userCourseToJson(const UserCourseRow& userCourse)
{
    Object::Ptr payload = new Object();
    payload->set("enrollmentId", userCourse.enrollmentId);
    payload->set("courseId", userCourse.courseId);
    payload->set("title", userCourse.title);
    payload->set("description", userCourse.description);
    payload->set("courseStatus", userCourse.courseStatus);
    payload->set("enrollmentStatus", userCourse.enrollmentStatus);
    payload->set("enrolledAt", userCourse.enrolledAt);
    return payload;
}

Object::Ptr completionToJson(const CompletionResult& completion)
{
    Object::Ptr payload = new Object();
    payload->set("userId", completion.userId);
    payload->set("lessonId", completion.lessonId);
    payload->set("completedAt", completion.completedAt);
    payload->set("alreadyCompleted", completion.alreadyCompleted);
    payload->set("enrollmentStatus", completion.enrollmentStatus);
    return payload;
}

Object::Ptr parseJsonBody(HTTPServerRequest& request)
{
    std::string body;
    Poco::StreamCopier::copyToString(request.stream(), body);

    if (trim(body).empty())
    {
        return new Object();
    }

    try
    {
        Parser parser;
        const auto parsed = parser.parse(body);
        return parsed.extract<Object::Ptr>();
    }
    catch (...)
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "invalid_json", "Request body must contain valid JSON object");
    }
}

void sendJson(HTTPServerResponse& response, HTTPResponse::HTTPStatus status, const Object::Ptr& payload)
{
    response.setStatus(status);
    response.setContentType("application/json; charset=utf-8");
    std::ostream& output = response.send();
    payload->stringify(output, 2);
}

void sendError(
    HTTPServerResponse& response,
    HTTPResponse::HTTPStatus status,
    const std::string& code,
    const std::string& message)
{
    Object::Ptr error = new Object();
    error->set("code", code);
    error->set("message", message);

    Object::Ptr payload = new Object();
    payload->set("error", error);

    sendJson(response, status, payload);
}

Database::Database(std::string dbPath): _dbPath(std::move(dbPath))
{
}

void Database::initialize()
{
    std::lock_guard<std::mutex> lock(_mutex);

    const std::filesystem::path databasePath(_dbPath);
    if (databasePath.has_parent_path())
    {
        std::filesystem::create_directories(databasePath.parent_path());
    }

    Session session = openSession();

    session << R"SQL(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            login TEXT NOT NULL UNIQUE,
            first_name TEXT NOT NULL,
            last_name TEXT NOT NULL,
            email TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL,
            status TEXT NOT NULL,
            created_at TEXT NOT NULL
        )
    )SQL", now;

    session << R"SQL(
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            token TEXT NOT NULL UNIQUE,
            user_id INTEGER NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        )
    )SQL", now;

    session << R"SQL(
        CREATE TABLE IF NOT EXISTS courses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            description TEXT NOT NULL,
            author_id INTEGER NOT NULL,
            status TEXT NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (author_id) REFERENCES users(id)
        )
    )SQL", now;

    session << R"SQL(
        CREATE TABLE IF NOT EXISTS lessons (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            course_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            description TEXT NOT NULL,
            position INTEGER NOT NULL,
            created_at TEXT NOT NULL,
            FOREIGN KEY (course_id) REFERENCES courses(id) ON DELETE CASCADE,
            UNIQUE (course_id, position)
        )
    )SQL", now;

    session << R"SQL(
        CREATE TABLE IF NOT EXISTS enrollments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            course_id INTEGER NOT NULL,
            status TEXT NOT NULL,
            enrolled_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY (course_id) REFERENCES courses(id) ON DELETE CASCADE,
            UNIQUE (user_id, course_id)
        )
    )SQL", now;

    session << R"SQL(
        CREATE TABLE IF NOT EXISTS lesson_completions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            lesson_id INTEGER NOT NULL,
            completed_at TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY (lesson_id) REFERENCES lessons(id) ON DELETE CASCADE,
            UNIQUE (user_id, lesson_id)
        )
    )SQL", now;

    seedUser("admin", "System", "Administrator", "admin@example.com", "admin123", ROLE_ADMIN, USER_STATUS_ACTIVE);
    seedUser("instructor", "Default", "Instructor", "instructor@example.com", "instructor123", ROLE_INSTRUCTOR, USER_STATUS_ACTIVE);
}

UserRow Database::createUser(
    const std::string& login,
    const std::string& firstName,
    const std::string& lastName,
    const std::string& email,
    const std::string& password,
    const std::string& role,
    const std::string& status)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    ensureLoginAvailable(session, login);
    ensureEmailAvailable(session, email);

    const auto createdAt = nowIso();
    const auto passwordHash = hashPassword(password);

    session << R"SQL(
        INSERT INTO users (login, first_name, last_name, email, password_hash, role, status, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )SQL",
        use(login),
        use(firstName),
        use(lastName),
        use(email),
        use(passwordHash),
        use(role),
        use(status),
        use(createdAt),
        now;

    int userId = 0;
    session << "SELECT last_insert_rowid()", into(userId), now;
    return getUserByIdLocked(session, userId);
}

std::optional<UserRow> Database::findUserByLogin(const std::string& login)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    std::vector<int> ids;
    std::vector<std::string> logins;
    std::vector<std::string> firstNames;
    std::vector<std::string> lastNames;
    std::vector<std::string> emails;
    std::vector<std::string> roles;
    std::vector<std::string> statuses;
    std::vector<std::string> createdAts;

    session << R"SQL(
        SELECT id, login, first_name, last_name, email, role, status, created_at
        FROM users
        WHERE login = ?
        LIMIT 1
    )SQL",
        into(ids),
        into(logins),
        into(firstNames),
        into(lastNames),
        into(emails),
        into(roles),
        into(statuses),
        into(createdAts),
        use(login),
        now;

    if (ids.empty())
    {
        return std::nullopt;
    }

    return UserRow{ids.front(), logins.front(), firstNames.front(), lastNames.front(), emails.front(), roles.front(), statuses.front(), createdAts.front()};
}

std::vector<UserRow> Database::searchUsers(const std::string& firstNameMask, const std::string& lastNameMask)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    std::vector<int> ids;
    std::vector<std::string> logins;
    std::vector<std::string> firstNames;
    std::vector<std::string> lastNames;
    std::vector<std::string> emails;
    std::vector<std::string> roles;
    std::vector<std::string> statuses;
    std::vector<std::string> createdAts;

    session << R"SQL(
        SELECT id, login, first_name, last_name, email, role, status, created_at
        FROM users
        WHERE LOWER(first_name) LIKE '%' || LOWER(?) || '%'
          AND LOWER(last_name) LIKE '%' || LOWER(?) || '%'
        ORDER BY id
    )SQL",
        into(ids),
        into(logins),
        into(firstNames),
        into(lastNames),
        into(emails),
        into(roles),
        into(statuses),
        into(createdAts),
        use(firstNameMask),
        use(lastNameMask),
        now;

    std::vector<UserRow> result;
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        result.push_back(UserRow{
            ids[index],
            logins[index],
            firstNames[index],
            lastNames[index],
            emails[index],
            roles[index],
            statuses[index],
            createdAts[index]
        });
    }

    return result;
}

std::optional<UserRow> Database::authenticateUser(const std::string& login, const std::string& password)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();
    const auto passwordHash = hashPassword(password);

    std::vector<int> ids;
    std::vector<std::string> logins;
    std::vector<std::string> firstNames;
    std::vector<std::string> lastNames;
    std::vector<std::string> emails;
    std::vector<std::string> roles;
    std::vector<std::string> statuses;
    std::vector<std::string> createdAts;

    session << R"SQL(
        SELECT id, login, first_name, last_name, email, role, status, created_at
        FROM users
        WHERE login = ? AND password_hash = ?
        LIMIT 1
    )SQL",
        into(ids),
        into(logins),
        into(firstNames),
        into(lastNames),
        into(emails),
        into(roles),
        into(statuses),
        into(createdAts),
        use(login),
        use(passwordHash),
        now;

    if (ids.empty())
    {
        return std::nullopt;
    }

    if (statuses.front() != USER_STATUS_ACTIVE)
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "user_blocked", "User account is blocked");
    }

    return UserRow{ids.front(), logins.front(), firstNames.front(), lastNames.front(), emails.front(), roles.front(), statuses.front(), createdAts.front()};
}

std::string Database::createSessionToken(int userId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    const auto token = generateToken();
    const auto createdAt = nowIso();

    session << "INSERT INTO sessions (token, user_id, created_at) VALUES (?, ?, ?)",
        use(token),
        use(userId),
        use(createdAt),
        now;

    return token;
}

std::optional<AuthContext> Database::findSessionByToken(const std::string& token)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    std::vector<int> ids;
    std::vector<std::string> logins;
    std::vector<std::string> firstNames;
    std::vector<std::string> lastNames;
    std::vector<std::string> emails;
    std::vector<std::string> roles;
    std::vector<std::string> statuses;
    std::vector<std::string> createdAts;

    session << R"SQL(
        SELECT u.id, u.login, u.first_name, u.last_name, u.email, u.role, u.status, u.created_at
        FROM sessions s
        JOIN users u ON u.id = s.user_id
        WHERE s.token = ?
        LIMIT 1
    )SQL",
        into(ids),
        into(logins),
        into(firstNames),
        into(lastNames),
        into(emails),
        into(roles),
        into(statuses),
        into(createdAts),
        use(token),
        now;

    if (ids.empty())
    {
        return std::nullopt;
    }

    return AuthContext{ids.front(), logins.front(), firstNames.front(), lastNames.front(), emails.front(), roles.front(), statuses.front(), createdAts.front()};
}

CourseRow Database::createCourse(const std::string& title, const std::string& description, int authorId, const std::string& status)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    if (!userExistsLocked(session, authorId))
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "author_not_found", "Author not found");
    }

    const auto createdAt = nowIso();

    session << R"SQL(
        INSERT INTO courses (title, description, author_id, status, created_at)
        VALUES (?, ?, ?, ?, ?)
    )SQL",
        use(title),
        use(description),
        use(authorId),
        use(status),
        use(createdAt),
        now;

    int courseId = 0;
    session << "SELECT last_insert_rowid()", into(courseId), now;
    return getCourseByIdLocked(session, courseId);
}

std::optional<CourseRow> Database::findCourseById(int courseId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    if (!courseExistsLocked(session, courseId))
    {
        return std::nullopt;
    }

    return getCourseByIdLocked(session, courseId);
}

CourseRow Database::updateCourse(
    int courseId,
    const std::optional<std::string>& title,
    const std::optional<std::string>& description,
    const std::optional<std::string>& status)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    if (!courseExistsLocked(session, courseId))
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    const auto current = getCourseByIdLocked(session, courseId);
    const auto newTitle = title ? *title : current.title;
    const auto newDescription = description ? *description : current.description;
    const auto newStatus = status ? *status : current.status;

    session << R"SQL(
        UPDATE courses
        SET title = ?, description = ?, status = ?
        WHERE id = ?
    )SQL",
        use(newTitle),
        use(newDescription),
        use(newStatus),
        use(courseId),
        now;

    return getCourseByIdLocked(session, courseId);
}

std::vector<CourseRow> Database::listCourses(bool includeAll)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    std::vector<int> ids;
    std::vector<std::string> titles;
    std::vector<std::string> descriptions;
    std::vector<int> authorIds;
    std::vector<std::string> statuses;
    std::vector<std::string> createdAts;

    if (includeAll)
    {
        session << R"SQL(
            SELECT id, title, description, author_id, status, created_at
            FROM courses
            ORDER BY id
        )SQL",
            into(ids),
            into(titles),
            into(descriptions),
            into(authorIds),
            into(statuses),
            into(createdAts),
            now;
    }
    else
    {
        const auto published = COURSE_STATUS_PUBLISHED;
        session << R"SQL(
            SELECT id, title, description, author_id, status, created_at
            FROM courses
            WHERE status = ?
            ORDER BY id
        )SQL",
            into(ids),
            into(titles),
            into(descriptions),
            into(authorIds),
            into(statuses),
            into(createdAts),
            use(published),
            now;
    }

    std::vector<CourseRow> result;
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        result.push_back(CourseRow{
            ids[index],
            titles[index],
            descriptions[index],
            authorIds[index],
            statuses[index],
            createdAts[index]
        });
    }

    return result;
}

LessonRow Database::addLesson(int courseId, const std::string& title, const std::string& description, int position)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    if (!courseExistsLocked(session, courseId))
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    int positionCount = 0;
    session << "SELECT COUNT(*) FROM lessons WHERE course_id = ? AND position = ?",
        into(positionCount),
        use(courseId),
        use(position),
        now;

    if (positionCount > 0)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "lesson_position_exists", "Lesson position must be unique within the course");
    }

    const auto createdAt = nowIso();
    session << R"SQL(
        INSERT INTO lessons (course_id, title, description, position, created_at)
        VALUES (?, ?, ?, ?, ?)
    )SQL",
        use(courseId),
        use(title),
        use(description),
        use(position),
        use(createdAt),
        now;

    int lessonId = 0;
    session << "SELECT last_insert_rowid()", into(lessonId), now;
    return getLessonByIdLocked(session, lessonId);
}

std::vector<LessonRow> Database::listLessons(int courseId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    if (!courseExistsLocked(session, courseId))
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    std::vector<int> ids;
    std::vector<int> courseIds;
    std::vector<std::string> titles;
    std::vector<std::string> descriptions;
    std::vector<int> positions;
    std::vector<std::string> createdAts;

    session << R"SQL(
        SELECT id, course_id, title, description, position, created_at
        FROM lessons
        WHERE course_id = ?
        ORDER BY position
    )SQL",
        into(ids),
        into(courseIds),
        into(titles),
        into(descriptions),
        into(positions),
        into(createdAts),
        use(courseId),
        now;

    std::vector<LessonRow> result;
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        result.push_back(LessonRow{
            ids[index],
            courseIds[index],
            titles[index],
            descriptions[index],
            positions[index],
            createdAts[index]
        });
    }

    return result;
}

EnrollmentRow Database::enrollUser(int userId, int courseId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    const auto user = getUserByIdLocked(session, userId);
    const auto course = getCourseByIdLocked(session, courseId);

    if (user.status != USER_STATUS_ACTIVE)
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "user_blocked", "Only active users can enroll");
    }
    if (user.role != ROLE_STUDENT)
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "role_not_allowed", "Only students can enroll in courses");
    }
    if (course.status != COURSE_STATUS_PUBLISHED)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "course_not_available", "Only published courses can be enrolled");
    }

    int existingCount = 0;
    session << "SELECT COUNT(*) FROM enrollments WHERE user_id = ? AND course_id = ?",
        into(existingCount),
        use(userId),
        use(courseId),
        now;

    if (existingCount > 0)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "already_enrolled", "Student is already enrolled in the selected course");
    }

    const auto enrolledAt = nowIso();
    const auto status = ENROLLMENT_STATUS_ACTIVE;

    session << R"SQL(
        INSERT INTO enrollments (user_id, course_id, status, enrolled_at)
        VALUES (?, ?, ?, ?)
    )SQL",
        use(userId),
        use(courseId),
        use(status),
        use(enrolledAt),
        now;

    int enrollmentId = 0;
    session << "SELECT last_insert_rowid()", into(enrollmentId), now;
    return getEnrollmentByIdLocked(session, enrollmentId);
}

std::vector<UserCourseRow> Database::listUserCourses(int userId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    if (!userExistsLocked(session, userId))
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "user_not_found", "User not found");
    }

    std::vector<int> enrollmentIds;
    std::vector<int> courseIds;
    std::vector<std::string> titles;
    std::vector<std::string> descriptions;
    std::vector<std::string> courseStatuses;
    std::vector<std::string> enrollmentStatuses;
    std::vector<std::string> enrolledAts;

    session << R"SQL(
        SELECT e.id, c.id, c.title, c.description, c.status, e.status, e.enrolled_at
        FROM enrollments e
        JOIN courses c ON c.id = e.course_id
        WHERE e.user_id = ?
        ORDER BY e.id
    )SQL",
        into(enrollmentIds),
        into(courseIds),
        into(titles),
        into(descriptions),
        into(courseStatuses),
        into(enrollmentStatuses),
        into(enrolledAts),
        use(userId),
        now;

    std::vector<UserCourseRow> result;
    for (std::size_t index = 0; index < enrollmentIds.size(); ++index)
    {
        result.push_back(UserCourseRow{
            enrollmentIds[index],
            courseIds[index],
            titles[index],
            descriptions[index],
            courseStatuses[index],
            enrollmentStatuses[index],
            enrolledAts[index]
        });
    }

    return result;
}

CompletionResult Database::markLessonCompleted(int userId, int lessonId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();

    const auto lesson = getLessonByIdLocked(session, lessonId);
    getUserByIdLocked(session, userId);

    std::vector<int> enrollmentIds;
    std::vector<std::string> enrollmentStatuses;
    session << R"SQL(
        SELECT id, status
        FROM enrollments
        WHERE user_id = ? AND course_id = ?
        LIMIT 1
    )SQL",
        into(enrollmentIds),
        into(enrollmentStatuses),
        use(userId),
        use(lesson.courseId),
        now;

    if (enrollmentIds.empty())
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "not_enrolled", "Student must be enrolled in the course before completing lessons");
    }

    int completionCount = 0;
    session << "SELECT COUNT(*) FROM lesson_completions WHERE user_id = ? AND lesson_id = ?",
        into(completionCount),
        use(userId),
        use(lessonId),
        now;

    CompletionResult result{};
    result.userId = userId;
    result.lessonId = lessonId;
    result.alreadyCompleted = completionCount > 0;

    if (completionCount == 0)
    {
        result.completedAt = nowIso();
        session << "INSERT INTO lesson_completions (user_id, lesson_id, completed_at) VALUES (?, ?, ?)",
            use(userId),
            use(lessonId),
            use(result.completedAt),
            now;
    }
    else
    {
        std::vector<std::string> completedAts;
        session << "SELECT completed_at FROM lesson_completions WHERE user_id = ? AND lesson_id = ? LIMIT 1",
            into(completedAts),
            use(userId),
            use(lessonId),
            now;

        result.completedAt = completedAts.empty() ? nowIso() : completedAts.front();
    }

    int lessonCount = 0;
    int completedCount = 0;
    session << "SELECT COUNT(*) FROM lessons WHERE course_id = ?",
        into(lessonCount),
        use(lesson.courseId),
        now;

    session << R"SQL(
        SELECT COUNT(*)
        FROM lesson_completions lc
        JOIN lessons l ON l.id = lc.lesson_id
        WHERE lc.user_id = ? AND l.course_id = ?
    )SQL",
        into(completedCount),
        use(userId),
        use(lesson.courseId),
        now;

    result.enrollmentStatus = enrollmentStatuses.front();
    if (lessonCount > 0 && completedCount >= lessonCount)
    {
        result.enrollmentStatus = ENROLLMENT_STATUS_COMPLETED;
        session << "UPDATE enrollments SET status = ? WHERE id = ?",
            use(result.enrollmentStatus),
            use(enrollmentIds.front()),
            now;
    }

    return result;
}

bool Database::isCourseOwnedBy(int courseId, int authorId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Session session = openSession();
    int count = 0;
    session << "SELECT COUNT(*) FROM courses WHERE id = ? AND author_id = ?",
        into(count),
        use(courseId),
        use(authorId),
        now;
    return count > 0;
}

Session Database::openSession() const
{
    Session session(Poco::Data::SQLite::Connector::KEY, _dbPath);
    session << "PRAGMA foreign_keys = ON", now;
    return session;
}

void Database::ensureLoginAvailable(Session& session, const std::string& login)
{
    int count = 0;
    session << "SELECT COUNT(*) FROM users WHERE login = ?",
        into(count),
        use(login),
        now;

    if (count > 0)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "login_exists", "User with this login already exists");
    }
}

void Database::ensureEmailAvailable(Session& session, const std::string& email)
{
    int count = 0;
    session << "SELECT COUNT(*) FROM users WHERE email = ?",
        into(count),
        use(email),
        now;

    if (count > 0)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "email_exists", "User with this email already exists");
    }
}

bool Database::userExistsLocked(Session& session, int userId)
{
    int count = 0;
    session << "SELECT COUNT(*) FROM users WHERE id = ?",
        into(count),
        use(userId),
        now;
    return count > 0;
}

bool Database::courseExistsLocked(Session& session, int courseId)
{
    int count = 0;
    session << "SELECT COUNT(*) FROM courses WHERE id = ?",
        into(count),
        use(courseId),
        now;
    return count > 0;
}

UserRow Database::getUserByIdLocked(Session& session, int userId)
{
    std::vector<int> ids;
    std::vector<std::string> logins;
    std::vector<std::string> firstNames;
    std::vector<std::string> lastNames;
    std::vector<std::string> emails;
    std::vector<std::string> roles;
    std::vector<std::string> statuses;
    std::vector<std::string> createdAts;

    session << R"SQL(
        SELECT id, login, first_name, last_name, email, role, status, created_at
        FROM users
        WHERE id = ?
        LIMIT 1
    )SQL",
        into(ids),
        into(logins),
        into(firstNames),
        into(lastNames),
        into(emails),
        into(roles),
        into(statuses),
        into(createdAts),
        use(userId),
        now;

    if (ids.empty())
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "user_not_found", "User not found");
    }

    return UserRow{ids.front(), logins.front(), firstNames.front(), lastNames.front(), emails.front(), roles.front(), statuses.front(), createdAts.front()};
}

CourseRow Database::getCourseByIdLocked(Session& session, int courseId)
{
    std::vector<int> ids;
    std::vector<std::string> titles;
    std::vector<std::string> descriptions;
    std::vector<int> authorIds;
    std::vector<std::string> statuses;
    std::vector<std::string> createdAts;

    session << R"SQL(
        SELECT id, title, description, author_id, status, created_at
        FROM courses
        WHERE id = ?
        LIMIT 1
    )SQL",
        into(ids),
        into(titles),
        into(descriptions),
        into(authorIds),
        into(statuses),
        into(createdAts),
        use(courseId),
        now;

    if (ids.empty())
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    return CourseRow{ids.front(), titles.front(), descriptions.front(), authorIds.front(), statuses.front(), createdAts.front()};
}

LessonRow Database::getLessonByIdLocked(Session& session, int lessonId)
{
    std::vector<int> ids;
    std::vector<int> courseIds;
    std::vector<std::string> titles;
    std::vector<std::string> descriptions;
    std::vector<int> positions;
    std::vector<std::string> createdAts;

    session << R"SQL(
        SELECT id, course_id, title, description, position, created_at
        FROM lessons
        WHERE id = ?
        LIMIT 1
    )SQL",
        into(ids),
        into(courseIds),
        into(titles),
        into(descriptions),
        into(positions),
        into(createdAts),
        use(lessonId),
        now;

    if (ids.empty())
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "lesson_not_found", "Lesson not found");
    }

    return LessonRow{ids.front(), courseIds.front(), titles.front(), descriptions.front(), positions.front(), createdAts.front()};
}

EnrollmentRow Database::getEnrollmentByIdLocked(Session& session, int enrollmentId)
{
    std::vector<int> ids;
    std::vector<int> userIds;
    std::vector<int> courseIds;
    std::vector<std::string> statuses;
    std::vector<std::string> enrolledAts;

    session << R"SQL(
        SELECT id, user_id, course_id, status, enrolled_at
        FROM enrollments
        WHERE id = ?
        LIMIT 1
    )SQL",
        into(ids),
        into(userIds),
        into(courseIds),
        into(statuses),
        into(enrolledAts),
        use(enrollmentId),
        now;

    if (ids.empty())
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "enrollment_not_found", "Enrollment not found");
    }

    return EnrollmentRow{ids.front(), userIds.front(), courseIds.front(), statuses.front(), enrolledAts.front()};
}

void Database::seedUser(
    const std::string& login,
    const std::string& firstName,
    const std::string& lastName,
    const std::string& email,
    const std::string& password,
    const std::string& role,
    const std::string& status)
{
    Session session = openSession();
    int count = 0;
    session << "SELECT COUNT(*) FROM users WHERE login = ?",
        into(count),
        use(login),
        now;

    if (count == 0)
    {
        const auto passwordHash = hashPassword(password);
        const auto createdAt = nowIso();
        session << R"SQL(
            INSERT INTO users (login, first_name, last_name, email, password_hash, role, status, created_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )SQL",
            use(login),
            use(firstName),
            use(lastName),
            use(email),
            use(passwordHash),
            use(role),
            use(status),
            use(createdAt),
            now;
    }
}

} // namespace lms
