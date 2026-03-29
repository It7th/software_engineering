#pragma once

#include <Poco/Data/Session.h>
#include <Poco/Data/Statement.h>
#include <Poco/JSON/Object.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/URI.h>
#include <Poco/Util/ServerApplication.h>

#include <initializer_list>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lms
{

template <typename T>
auto use(const T& value)
{
    return Poco::Data::Keywords::use(const_cast<T&>(value));
}

inline const std::string ROLE_ADMIN = "ADMIN";
inline const std::string ROLE_INSTRUCTOR = "INSTRUCTOR";
inline const std::string ROLE_STUDENT = "STUDENT";

inline const std::string USER_STATUS_ACTIVE = "ACTIVE";
inline const std::string USER_STATUS_BLOCKED = "BLOCKED";

inline const std::string COURSE_STATUS_DRAFT = "DRAFT";
inline const std::string COURSE_STATUS_PUBLISHED = "PUBLISHED";
inline const std::string COURSE_STATUS_ARCHIVED = "ARCHIVED";

inline const std::string ENROLLMENT_STATUS_ACTIVE = "ACTIVE";
inline const std::string ENROLLMENT_STATUS_COMPLETED = "COMPLETED";
inline const std::string ENROLLMENT_STATUS_CANCELLED = "CANCELLED";

struct ApiError : public std::runtime_error
{
    ApiError(
        Poco::Net::HTTPResponse::HTTPStatus httpStatus,
        std::string errorCode,
        std::string errorMessage):
        std::runtime_error(errorMessage),
        status(httpStatus),
        code(std::move(errorCode))
    {
    }

    Poco::Net::HTTPResponse::HTTPStatus status;
    std::string code;
};

struct UserRow
{
    int id{};
    std::string login;
    std::string firstName;
    std::string lastName;
    std::string email;
    std::string role;
    std::string status;
    std::string createdAt;
};

struct CourseRow
{
    int id{};
    std::string title;
    std::string description;
    int authorId{};
    std::string status;
    std::string createdAt;
};

struct LessonRow
{
    int id{};
    int courseId{};
    std::string title;
    std::string description;
    int position{};
    std::string createdAt;
};

struct EnrollmentRow
{
    int id{};
    int userId{};
    int courseId{};
    std::string status;
    std::string enrolledAt;
};

struct UserCourseRow
{
    int enrollmentId{};
    int courseId{};
    std::string title;
    std::string description;
    std::string courseStatus;
    std::string enrollmentStatus;
    std::string enrolledAt;
};

struct CompletionResult
{
    int userId{};
    int lessonId{};
    std::string completedAt;
    bool alreadyCompleted{};
    std::string enrollmentStatus;
};

using AuthContext = UserRow;

std::string trim(const std::string& value);
std::string toUpper(std::string value);
std::string toLower(std::string value);
bool isTruthy(std::string value);
std::string nowIso();
std::string hashPassword(const std::string& password);
std::string generateToken();
int parseInt(const std::string& value, const std::string& fieldName);
std::map<std::string, std::string> queryParams(const Poco::URI& uri);
std::string requireString(const Poco::JSON::Object::Ptr& body, const std::string& fieldName);
std::optional<std::string> optionalString(const Poco::JSON::Object::Ptr& body, const std::string& fieldName);
int requireInt(const Poco::JSON::Object::Ptr& body, const std::string& fieldName);
std::string normalizeRole(const std::string& value);
std::string normalizeUserStatus(const std::string& value);
std::string normalizeCourseStatus(const std::string& value);

Poco::JSON::Object::Ptr userToJson(const UserRow& user);
Poco::JSON::Object::Ptr courseToJson(const CourseRow& course);
Poco::JSON::Object::Ptr lessonToJson(const LessonRow& lesson);
Poco::JSON::Object::Ptr enrollmentToJson(const EnrollmentRow& enrollment);
Poco::JSON::Object::Ptr userCourseToJson(const UserCourseRow& userCourse);
Poco::JSON::Object::Ptr completionToJson(const CompletionResult& completion);
Poco::JSON::Object::Ptr parseJsonBody(Poco::Net::HTTPServerRequest& request);
void sendJson(
    Poco::Net::HTTPServerResponse& response,
    Poco::Net::HTTPResponse::HTTPStatus status,
    const Poco::JSON::Object::Ptr& payload);
void sendError(
    Poco::Net::HTTPServerResponse& response,
    Poco::Net::HTTPResponse::HTTPStatus status,
    const std::string& code,
    const std::string& message);

class Database
{
public:
    explicit Database(std::string dbPath);

    void initialize();
    UserRow createUser(
        const std::string& login,
        const std::string& firstName,
        const std::string& lastName,
        const std::string& email,
        const std::string& password,
        const std::string& role,
        const std::string& status);
    std::optional<UserRow> findUserByLogin(const std::string& login);
    std::vector<UserRow> searchUsers(const std::string& firstNameMask, const std::string& lastNameMask);
    std::optional<UserRow> authenticateUser(const std::string& login, const std::string& password);
    std::string createSessionToken(int userId);
    std::optional<AuthContext> findSessionByToken(const std::string& token);
    CourseRow createCourse(const std::string& title, const std::string& description, int authorId, const std::string& status);
    std::optional<CourseRow> findCourseById(int courseId);
    CourseRow updateCourse(
        int courseId,
        const std::optional<std::string>& title,
        const std::optional<std::string>& description,
        const std::optional<std::string>& status);
    std::vector<CourseRow> listCourses(bool includeAll);
    LessonRow addLesson(int courseId, const std::string& title, const std::string& description, int position);
    std::vector<LessonRow> listLessons(int courseId);
    EnrollmentRow enrollUser(int userId, int courseId);
    std::vector<UserCourseRow> listUserCourses(int userId);
    CompletionResult markLessonCompleted(int userId, int lessonId);
    bool isCourseOwnedBy(int courseId, int authorId);

private:
    Poco::Data::Session openSession() const;
    void ensureLoginAvailable(Poco::Data::Session& session, const std::string& login);
    void ensureEmailAvailable(Poco::Data::Session& session, const std::string& email);
    bool userExistsLocked(Poco::Data::Session& session, int userId);
    bool courseExistsLocked(Poco::Data::Session& session, int courseId);
    UserRow getUserByIdLocked(Poco::Data::Session& session, int userId);
    CourseRow getCourseByIdLocked(Poco::Data::Session& session, int courseId);
    LessonRow getLessonByIdLocked(Poco::Data::Session& session, int lessonId);
    EnrollmentRow getEnrollmentByIdLocked(Poco::Data::Session& session, int enrollmentId);
    void seedUser(
        const std::string& login,
        const std::string& firstName,
        const std::string& lastName,
        const std::string& email,
        const std::string& password,
        const std::string& role,
        const std::string& status);

    std::string _dbPath;
    mutable std::mutex _mutex;
};

class ApiRequestHandler : public Poco::Net::HTTPRequestHandler
{
public:
    explicit ApiRequestHandler(Database& database);

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
    void handleHealth(Poco::Net::HTTPServerResponse& response);
    void handleRegister(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);
    void handleLogin(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);
    void handleCreateUser(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);
    void handleFindUserByLogin(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        const Poco::URI& uri);
    void handleSearchUsers(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        const Poco::URI& uri);
    void handleCreateCourse(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);
    void handleListCourses(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        const Poco::URI& uri);
    void handlePatchCourse(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        int courseId);
    void handleAddLesson(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        int courseId);
    void handleListLessons(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        int courseId);
    void handleEnroll(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        int courseId);
    void handleListUserCourses(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        int userId);
    void handleCompleteLesson(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        int userId,
        int lessonId);
    std::optional<AuthContext> tryAuthenticated(Poco::Net::HTTPServerRequest& request);
    AuthContext requireAuthenticated(Poco::Net::HTTPServerRequest& request);
    void requireAnyRole(const AuthContext& auth, const std::initializer_list<std::string>& roles);
    void ensureSelfOrAdmin(const AuthContext& auth, int userId);

    Database& _database;
};

class ApiRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    explicit ApiRequestHandlerFactory(Database& database);

    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;

private:
    Database& _database;
};

class LearningManagementApplication : public Poco::Util::ServerApplication
{
public:
    LearningManagementApplication();

protected:
    void initialize(Poco::Util::Application& self) override;
    void uninitialize() override;
    int main(const std::vector<std::string>& args) override;

private:
    static int resolvePort();
    static std::string resolveDatabasePath();

    Database _database;
};

} // namespace lms
