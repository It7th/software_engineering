#include "lms.hpp"

#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DigestEngine.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>
#include <Poco/NumberParser.h>
#include <Poco/SHA2Engine.h>
#include <Poco/StreamCopier.h>
#include <Poco/Timestamp.h>
#include <Poco/UUIDGenerator.h>

#include <algorithm>
#include <cctype>
#include <ostream>

using Poco::JSON::Object;
using Poco::JSON::Parser;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;

namespace lms
{

namespace
{

const std::string USER_SELECT_COLUMNS = R"SQL(
    id,
    login,
    first_name,
    last_name,
    email,
    role,
    status,
    to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"') AS created_at
)SQL";

const std::string COURSE_SELECT_COLUMNS = R"SQL(
    id,
    title,
    description,
    author_id,
    status,
    to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"') AS created_at
)SQL";

const std::string LESSON_SELECT_COLUMNS = R"SQL(
    id,
    course_id,
    title,
    description,
    position,
    to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"') AS created_at
)SQL";

const std::string ENROLLMENT_SELECT_COLUMNS = R"SQL(
    id,
    user_id,
    course_id,
    status,
    to_char(enrolled_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"') AS enrolled_at
)SQL";

UserRow mapUserRow(const pqxx::row& row)
{
    return UserRow{
        row["id"].as<int>(),
        row["login"].as<std::string>(),
        row["first_name"].as<std::string>(),
        row["last_name"].as<std::string>(),
        row["email"].as<std::string>(),
        row["role"].as<std::string>(),
        row["status"].as<std::string>(),
        row["created_at"].as<std::string>()
    };
}

CourseRow mapCourseRow(const pqxx::row& row)
{
    return CourseRow{
        row["id"].as<int>(),
        row["title"].as<std::string>(),
        row["description"].as<std::string>(),
        row["author_id"].as<int>(),
        row["status"].as<std::string>(),
        row["created_at"].as<std::string>()
    };
}

LessonRow mapLessonRow(const pqxx::row& row)
{
    return LessonRow{
        row["id"].as<int>(),
        row["course_id"].as<int>(),
        row["title"].as<std::string>(),
        row["description"].as<std::string>(),
        row["position"].as<int>(),
        row["created_at"].as<std::string>()
    };
}

EnrollmentRow mapEnrollmentRow(const pqxx::row& row)
{
    return EnrollmentRow{
        row["id"].as<int>(),
        row["user_id"].as<int>(),
        row["course_id"].as<int>(),
        row["status"].as<std::string>(),
        row["enrolled_at"].as<std::string>()
    };
}

} // namespace

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

Database::Database(std::string connectionString): _connectionString(std::move(connectionString))
{
}

void Database::initialize()
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    const bool schemaReady = txn.exec1(R"SQL(
        SELECT
            to_regclass('identity.users') IS NOT NULL
            AND to_regclass('auth.sessions') IS NOT NULL
            AND to_regclass('catalog.courses') IS NOT NULL
            AND to_regclass('catalog.lessons') IS NOT NULL
            AND to_regclass('learning.enrollments') IS NOT NULL
            AND to_regclass('learning.lesson_completions') IS NOT NULL
    )SQL")[0].as<bool>();

    if (!schemaReady)
    {
        throw std::runtime_error("Database schema is not initialized. Run schema.sql and data.sql before starting the API.");
    }
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
    pqxx::connection connection = openConnection();
    pqxx::work txn(connection);

    ensureLoginAvailable(txn, login);
    ensureEmailAvailable(txn, email);

    const pqxx::result result = txn.exec_params(
        std::string("INSERT INTO identity.users (login, first_name, last_name, email, password_hash, role, status) ")
            + "VALUES ($1, $2, $3, $4, $5, $6, $7) "
            + "RETURNING " + USER_SELECT_COLUMNS,
        login,
        firstName,
        lastName,
        email,
        hashPassword(password),
        role,
        status);

    const auto user = mapUserRow(result[0]);
    txn.commit();
    return user;
}

std::optional<UserRow> Database::findUserByLogin(const std::string& login)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + USER_SELECT_COLUMNS + " "
            + "FROM identity.users "
            + "WHERE login = $1 "
            + "LIMIT 1",
        login);

    if (result.empty())
    {
        return std::nullopt;
    }

    return mapUserRow(result[0]);
}

std::vector<UserRow> Database::searchUsers(const std::string& firstNameMask, const std::string& lastNameMask)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + USER_SELECT_COLUMNS + " "
            + "FROM identity.users "
            + "WHERE first_name ILIKE ('%' || $1 || '%') "
            + "  AND last_name ILIKE ('%' || $2 || '%') "
            + "ORDER BY id",
        firstNameMask,
        lastNameMask);

    std::vector<UserRow> users;
    users.reserve(result.size());
    for (const auto& row : result)
    {
        users.push_back(mapUserRow(row));
    }

    return users;
}

std::optional<UserRow> Database::authenticateUser(const std::string& login, const std::string& password)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + USER_SELECT_COLUMNS + " "
            + "FROM identity.users "
            + "WHERE login = $1 AND password_hash = $2 "
            + "LIMIT 1",
        login,
        hashPassword(password));

    if (result.empty())
    {
        return std::nullopt;
    }

    const auto user = mapUserRow(result[0]);
    if (user.status != USER_STATUS_ACTIVE)
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "user_blocked", "User account is blocked");
    }

    return user;
}

std::string Database::createSessionToken(int userId)
{
    pqxx::connection connection = openConnection();
    pqxx::work txn(connection);

    getUserById(txn, userId);

    const auto token = generateToken();
    txn.exec_params(
        "INSERT INTO auth.sessions (token, user_id) VALUES ($1, $2)",
        token,
        userId);

    txn.commit();
    return token;
}

std::optional<AuthContext> Database::findSessionByToken(const std::string& token)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    const pqxx::result result = txn.exec_params(
        "SELECT "
        "u.id, u.login, u.first_name, u.last_name, u.email, u.role, u.status, "
        "to_char(u.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') AS created_at "
        "FROM auth.sessions s "
        "JOIN identity.users u ON u.id = s.user_id "
        "WHERE s.token = $1 "
        "LIMIT 1",
        token);

    if (result.empty())
    {
        return std::nullopt;
    }

    return mapUserRow(result[0]);
}

CourseRow Database::createCourse(const std::string& title, const std::string& description, int authorId, const std::string& status)
{
    pqxx::connection connection = openConnection();
    pqxx::work txn(connection);

    getUserById(txn, authorId);

    const pqxx::result result = txn.exec_params(
        std::string("INSERT INTO catalog.courses (title, description, author_id, status) ")
            + "VALUES ($1, $2, $3, $4) "
            + "RETURNING " + COURSE_SELECT_COLUMNS,
        title,
        description,
        authorId,
        status);

    const auto course = mapCourseRow(result[0]);
    txn.commit();
    return course;
}

std::optional<CourseRow> Database::findCourseById(int courseId)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + COURSE_SELECT_COLUMNS + " "
            + "FROM catalog.courses "
            + "WHERE id = $1 "
            + "LIMIT 1",
        courseId);

    if (result.empty())
    {
        return std::nullopt;
    }

    return mapCourseRow(result[0]);
}

CourseRow Database::updateCourse(
    int courseId,
    const std::optional<std::string>& title,
    const std::optional<std::string>& description,
    const std::optional<std::string>& status)
{
    pqxx::connection connection = openConnection();
    pqxx::work txn(connection);

    const auto current = getCourseById(txn, courseId);

    const pqxx::result result = txn.exec_params(
        std::string("UPDATE catalog.courses ")
            + "SET title = $1, description = $2, status = $3 "
            + "WHERE id = $4 "
            + "RETURNING " + COURSE_SELECT_COLUMNS,
        title ? *title : current.title,
        description ? *description : current.description,
        status ? *status : current.status,
        courseId);

    const auto updated = mapCourseRow(result[0]);
    txn.commit();
    return updated;
}

std::vector<CourseRow> Database::listCourses(bool includeAll)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    pqxx::result result;
    if (includeAll)
    {
        result = txn.exec(
            std::string("SELECT ") + COURSE_SELECT_COLUMNS + " "
                + "FROM catalog.courses "
                + "ORDER BY id");
    }
    else
    {
        result = txn.exec_params(
            std::string("SELECT ") + COURSE_SELECT_COLUMNS + " "
                + "FROM catalog.courses "
                + "WHERE status = $1 "
                + "ORDER BY id",
            COURSE_STATUS_PUBLISHED);
    }

    std::vector<CourseRow> courses;
    courses.reserve(result.size());
    for (const auto& row : result)
    {
        courses.push_back(mapCourseRow(row));
    }

    return courses;
}

LessonRow Database::addLesson(int courseId, const std::string& title, const std::string& description, int position)
{
    if (position <= 0)
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", "position must be greater than zero");
    }

    pqxx::connection connection = openConnection();
    pqxx::work txn(connection);

    getCourseById(txn, courseId);

    const auto duplicatePosition = txn.exec_params1(
        "SELECT COUNT(*) FROM catalog.lessons WHERE course_id = $1 AND position = $2",
        courseId,
        position)[0].as<int>();

    if (duplicatePosition > 0)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "lesson_position_exists", "Lesson position must be unique within the course");
    }

    const pqxx::result result = txn.exec_params(
        std::string("INSERT INTO catalog.lessons (course_id, title, description, position) ")
            + "VALUES ($1, $2, $3, $4) "
            + "RETURNING " + LESSON_SELECT_COLUMNS,
        courseId,
        title,
        description,
        position);

    const auto lesson = mapLessonRow(result[0]);
    txn.commit();
    return lesson;
}

std::vector<LessonRow> Database::listLessons(int courseId)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    if (!courseExists(txn, courseId))
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + LESSON_SELECT_COLUMNS + " "
            + "FROM catalog.lessons "
            + "WHERE course_id = $1 "
            + "ORDER BY position",
        courseId);

    std::vector<LessonRow> lessons;
    lessons.reserve(result.size());
    for (const auto& row : result)
    {
        lessons.push_back(mapLessonRow(row));
    }

    return lessons;
}

EnrollmentRow Database::enrollUser(int userId, int courseId)
{
    pqxx::connection connection = openConnection();
    pqxx::work txn(connection);

    const auto user = getUserById(txn, userId);
    const auto course = getCourseById(txn, courseId);

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

    const auto existing = txn.exec_params1(
        "SELECT COUNT(*) FROM learning.enrollments WHERE user_id = $1 AND course_id = $2",
        userId,
        courseId)[0].as<int>();

    if (existing > 0)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "already_enrolled", "Student is already enrolled in the selected course");
    }

    const pqxx::result result = txn.exec_params(
        std::string("INSERT INTO learning.enrollments (user_id, course_id, status) ")
            + "VALUES ($1, $2, $3) "
            + "RETURNING " + ENROLLMENT_SELECT_COLUMNS,
        userId,
        courseId,
        ENROLLMENT_STATUS_ACTIVE);

    const auto enrollment = mapEnrollmentRow(result[0]);
    txn.commit();
    return enrollment;
}

std::vector<UserCourseRow> Database::listUserCourses(int userId)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    if (!userExists(txn, userId))
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "user_not_found", "User not found");
    }

    const pqxx::result result = txn.exec_params(R"SQL(
        SELECT
            e.id AS enrollment_id,
            c.id AS course_id,
            c.title,
            c.description,
            c.status AS course_status,
            e.status AS enrollment_status,
            to_char(e.enrolled_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"') AS enrolled_at
        FROM learning.enrollments e
        JOIN catalog.courses c ON c.id = e.course_id
        WHERE e.user_id = $1
        ORDER BY e.id
    )SQL", userId);

    std::vector<UserCourseRow> courses;
    courses.reserve(result.size());
    for (const auto& row : result)
    {
        courses.push_back(UserCourseRow{
            row["enrollment_id"].as<int>(),
            row["course_id"].as<int>(),
            row["title"].as<std::string>(),
            row["description"].as<std::string>(),
            row["course_status"].as<std::string>(),
            row["enrollment_status"].as<std::string>(),
            row["enrolled_at"].as<std::string>()
        });
    }

    return courses;
}

CompletionResult Database::markLessonCompleted(int userId, int lessonId)
{
    pqxx::connection connection = openConnection();
    pqxx::work txn(connection);

    getUserById(txn, userId);
    const auto lesson = getLessonById(txn, lessonId);

    const pqxx::result enrollmentResult = txn.exec_params(
        "SELECT id, status "
        "FROM learning.enrollments "
        "WHERE user_id = $1 AND course_id = $2 "
        "LIMIT 1",
        userId,
        lesson.courseId);

    if (enrollmentResult.empty())
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "not_enrolled", "Student must be enrolled in the course before completing lessons");
    }

    CompletionResult result{};
    result.userId = userId;
    result.lessonId = lessonId;
    result.enrollmentStatus = enrollmentResult[0]["status"].as<std::string>();

    const pqxx::result insertResult = txn.exec_params(R"SQL(
        INSERT INTO learning.lesson_completions (user_id, lesson_id)
        VALUES ($1, $2)
        ON CONFLICT (user_id, lesson_id) DO NOTHING
        RETURNING to_char(completed_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"') AS completed_at
    )SQL", userId, lessonId);

    result.alreadyCompleted = insertResult.empty();
    if (result.alreadyCompleted)
    {
        result.completedAt = txn.exec_params1(R"SQL(
            SELECT to_char(completed_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"')
            FROM learning.lesson_completions
            WHERE user_id = $1 AND lesson_id = $2
            LIMIT 1
        )SQL", userId, lessonId)[0].as<std::string>();
    }
    else
    {
        result.completedAt = insertResult[0]["completed_at"].as<std::string>();
    }

    const auto lessonCount = txn.exec_params1(
        "SELECT COUNT(*) FROM catalog.lessons WHERE course_id = $1",
        lesson.courseId)[0].as<int>();

    const auto completedCount = txn.exec_params1(R"SQL(
        SELECT COUNT(*)
        FROM catalog.lessons l
        JOIN learning.lesson_completions lc
          ON lc.lesson_id = l.id
         AND lc.user_id = $1
        WHERE l.course_id = $2
    )SQL", userId, lesson.courseId)[0].as<int>();

    if (lessonCount > 0 && completedCount >= lessonCount && result.enrollmentStatus != ENROLLMENT_STATUS_COMPLETED)
    {
        txn.exec_params(
            "UPDATE learning.enrollments SET status = $1 WHERE id = $2",
            ENROLLMENT_STATUS_COMPLETED,
            enrollmentResult[0]["id"].as<int>());
        result.enrollmentStatus = ENROLLMENT_STATUS_COMPLETED;
    }

    txn.commit();
    return result;
}

bool Database::isCourseOwnedBy(int courseId, int authorId)
{
    pqxx::connection connection = openConnection();
    pqxx::read_transaction txn(connection);

    return txn.exec_params1(
        "SELECT EXISTS(SELECT 1 FROM catalog.courses WHERE id = $1 AND author_id = $2)",
        courseId,
        authorId)[0].as<bool>();
}

pqxx::connection Database::openConnection() const
{
    return pqxx::connection(_connectionString);
}

void Database::ensureLoginAvailable(pqxx::transaction_base& txn, const std::string& login)
{
    const auto count = txn.exec_params1(
        "SELECT COUNT(*) FROM identity.users WHERE login = $1",
        login)[0].as<int>();

    if (count > 0)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "login_exists", "User with this login already exists");
    }
}

void Database::ensureEmailAvailable(pqxx::transaction_base& txn, const std::string& email)
{
    const auto count = txn.exec_params1(
        "SELECT COUNT(*) FROM identity.users WHERE email = $1",
        email)[0].as<int>();

    if (count > 0)
    {
        throw ApiError(HTTPResponse::HTTP_CONFLICT, "email_exists", "User with this email already exists");
    }
}

bool Database::userExists(pqxx::transaction_base& txn, int userId)
{
    return txn.exec_params1(
        "SELECT EXISTS(SELECT 1 FROM identity.users WHERE id = $1)",
        userId)[0].as<bool>();
}

bool Database::courseExists(pqxx::transaction_base& txn, int courseId)
{
    return txn.exec_params1(
        "SELECT EXISTS(SELECT 1 FROM catalog.courses WHERE id = $1)",
        courseId)[0].as<bool>();
}

UserRow Database::getUserById(pqxx::transaction_base& txn, int userId)
{
    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + USER_SELECT_COLUMNS + " "
            + "FROM identity.users "
            + "WHERE id = $1 "
            + "LIMIT 1",
        userId);

    if (result.empty())
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "user_not_found", "User not found");
    }

    return mapUserRow(result[0]);
}

CourseRow Database::getCourseById(pqxx::transaction_base& txn, int courseId)
{
    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + COURSE_SELECT_COLUMNS + " "
            + "FROM catalog.courses "
            + "WHERE id = $1 "
            + "LIMIT 1",
        courseId);

    if (result.empty())
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    return mapCourseRow(result[0]);
}

LessonRow Database::getLessonById(pqxx::transaction_base& txn, int lessonId)
{
    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + LESSON_SELECT_COLUMNS + " "
            + "FROM catalog.lessons "
            + "WHERE id = $1 "
            + "LIMIT 1",
        lessonId);

    if (result.empty())
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "lesson_not_found", "Lesson not found");
    }

    return mapLessonRow(result[0]);
}

EnrollmentRow Database::getEnrollmentById(pqxx::transaction_base& txn, int enrollmentId)
{
    const pqxx::result result = txn.exec_params(
        std::string("SELECT ") + ENROLLMENT_SELECT_COLUMNS + " "
            + "FROM learning.enrollments "
            + "WHERE id = $1 "
            + "LIMIT 1",
        enrollmentId);

    if (result.empty())
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "enrollment_not_found", "Enrollment not found");
    }

    return mapEnrollmentRow(result[0]);
}

} // namespace lms
