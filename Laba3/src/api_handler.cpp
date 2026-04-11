#include "lms.hpp"

#include <Poco/Exception.h>
#include <Poco/JSON/Array.h>
#include <Poco/Net/HTTPRequest.h>

#include <algorithm>

using Poco::JSON::Array;
using Poco::JSON::Object;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;

namespace lms
{

ApiRequestHandler::ApiRequestHandler(Database& database): _database(database)
{
}

void ApiRequestHandler::handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
{
    try
    {
        Poco::URI uri(request.getURI());
        std::vector<std::string> segments;
        uri.getPathSegments(segments);

        if (request.getMethod() == HTTPRequest::HTTP_GET && segments.size() == 1 && segments[0] == "health")
        {
            return handleHealth(response);
        }
        if (request.getMethod() == HTTPRequest::HTTP_POST && segments.size() == 2 && segments[0] == "auth" && segments[1] == "register")
        {
            return handleRegister(request, response);
        }
        if (request.getMethod() == HTTPRequest::HTTP_POST && segments.size() == 2 && segments[0] == "auth" && segments[1] == "login")
        {
            return handleLogin(request, response);
        }
        if (request.getMethod() == HTTPRequest::HTTP_POST && segments.size() == 1 && segments[0] == "users")
        {
            return handleCreateUser(request, response);
        }
        if (request.getMethod() == HTTPRequest::HTTP_GET && segments.size() == 2 && segments[0] == "users" && segments[1] == "by-login")
        {
            return handleFindUserByLogin(request, response, uri);
        }
        if (request.getMethod() == HTTPRequest::HTTP_GET && segments.size() == 2 && segments[0] == "users" && segments[1] == "search")
        {
            return handleSearchUsers(request, response, uri);
        }
        if (request.getMethod() == HTTPRequest::HTTP_POST && segments.size() == 1 && segments[0] == "courses")
        {
            return handleCreateCourse(request, response);
        }
        if (request.getMethod() == HTTPRequest::HTTP_GET && segments.size() == 1 && segments[0] == "courses")
        {
            return handleListCourses(request, response, uri);
        }
        if (request.getMethod() == HTTPRequest::HTTP_PATCH && segments.size() == 2 && segments[0] == "courses")
        {
            return handlePatchCourse(request, response, parseInt(segments[1], "courseId"));
        }
        if (request.getMethod() == HTTPRequest::HTTP_POST && segments.size() == 3 && segments[0] == "courses" && segments[2] == "lessons")
        {
            return handleAddLesson(request, response, parseInt(segments[1], "courseId"));
        }
        if (request.getMethod() == HTTPRequest::HTTP_GET && segments.size() == 3 && segments[0] == "courses" && segments[2] == "lessons")
        {
            return handleListLessons(request, response, parseInt(segments[1], "courseId"));
        }
        if (request.getMethod() == HTTPRequest::HTTP_POST && segments.size() == 3 && segments[0] == "courses" && segments[2] == "enrollments")
        {
            return handleEnroll(request, response, parseInt(segments[1], "courseId"));
        }
        if (request.getMethod() == HTTPRequest::HTTP_GET && segments.size() == 3 && segments[0] == "users" && segments[2] == "courses")
        {
            return handleListUserCourses(request, response, parseInt(segments[1], "userId"));
        }
        if (request.getMethod() == HTTPRequest::HTTP_POST && segments.size() == 5 && segments[0] == "users" && segments[2] == "lessons" && segments[4] == "completion")
        {
            return handleCompleteLesson(request, response, parseInt(segments[1], "userId"), parseInt(segments[3], "lessonId"));
        }

        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "route_not_found", "Endpoint not found");
    }
    catch (const ApiError& error)
    {
        sendError(response, error.status, error.code, error.what());
    }
    catch (const Poco::Exception& error)
    {
        sendError(response, HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, "poco_error", error.displayText());
    }
    catch (const std::exception& error)
    {
        sendError(response, HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, "internal_error", error.what());
    }
}

void ApiRequestHandler::handleHealth(HTTPServerResponse& response)
{
    Object::Ptr payload = new Object();
    payload->set("status", "ok");
    payload->set("service", "learning-management-api");
    payload->set("timestamp", nowIso());
    sendJson(response, HTTPResponse::HTTP_OK, payload);
}

void ApiRequestHandler::handleRegister(HTTPServerRequest& request, HTTPServerResponse& response)
{
    const auto body = parseJsonBody(request);
    const auto user = _database.createUser(
        requireString(body, "login"),
        requireString(body, "firstName"),
        requireString(body, "lastName"),
        requireString(body, "email"),
        requireString(body, "password"),
        ROLE_STUDENT,
        USER_STATUS_ACTIVE);

    Object::Ptr payload = new Object();
    payload->set("user", userToJson(user));
    sendJson(response, HTTPResponse::HTTP_CREATED, payload);
}

void ApiRequestHandler::handleLogin(HTTPServerRequest& request, HTTPServerResponse& response)
{
    const auto body = parseJsonBody(request);
    const auto login = requireString(body, "login");
    const auto password = requireString(body, "password");

    const auto user = _database.authenticateUser(login, password);
    if (!user)
    {
        throw ApiError(HTTPResponse::HTTP_UNAUTHORIZED, "invalid_credentials", "Invalid login or password");
    }

    const auto token = _database.createSessionToken(user->id);

    Object::Ptr payload = new Object();
    payload->set("token", token);
    payload->set("user", userToJson(*user));
    sendJson(response, HTTPResponse::HTTP_OK, payload);
}

void ApiRequestHandler::handleCreateUser(HTTPServerRequest& request, HTTPServerResponse& response)
{
    const auto auth = requireAuthenticated(request);
    requireAnyRole(auth, {ROLE_ADMIN});

    const auto body = parseJsonBody(request);
    const auto role = normalizeRole(optionalString(body, "role").value_or(ROLE_STUDENT));
    const auto status = normalizeUserStatus(optionalString(body, "status").value_or(USER_STATUS_ACTIVE));

    const auto user = _database.createUser(
        requireString(body, "login"),
        requireString(body, "firstName"),
        requireString(body, "lastName"),
        requireString(body, "email"),
        requireString(body, "password"),
        role,
        status);

    Object::Ptr payload = new Object();
    payload->set("user", userToJson(user));
    sendJson(response, HTTPResponse::HTTP_CREATED, payload);
}

void ApiRequestHandler::handleFindUserByLogin(HTTPServerRequest& request, HTTPServerResponse& response, const Poco::URI& uri)
{
    const auto auth = requireAuthenticated(request);
    requireAnyRole(auth, {ROLE_ADMIN});

    const auto params = queryParams(uri);
    const auto iterator = params.find("login");
    if (iterator == params.end() || trim(iterator->second).empty())
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", "login query parameter is required");
    }

    const auto user = _database.findUserByLogin(iterator->second);
    if (!user)
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "user_not_found", "User not found");
    }

    Object::Ptr payload = new Object();
    payload->set("user", userToJson(*user));
    sendJson(response, HTTPResponse::HTTP_OK, payload);
}

void ApiRequestHandler::handleSearchUsers(HTTPServerRequest& request, HTTPServerResponse& response, const Poco::URI& uri)
{
    const auto auth = requireAuthenticated(request);
    requireAnyRole(auth, {ROLE_ADMIN});

    const auto params = queryParams(uri);
    const auto firstNameMask = params.count("firstNameMask") > 0 ? params.at("firstNameMask") : "";
    const auto lastNameMask = params.count("lastNameMask") > 0 ? params.at("lastNameMask") : "";

    const auto users = _database.searchUsers(firstNameMask, lastNameMask);

    Array::Ptr items = new Array();
    for (const auto& user : users)
    {
        items->add(userToJson(user));
    }

    Object::Ptr payload = new Object();
    payload->set("count", static_cast<int>(users.size()));
    payload->set("items", items);
    sendJson(response, HTTPResponse::HTTP_OK, payload);
}

void ApiRequestHandler::handleCreateCourse(HTTPServerRequest& request, HTTPServerResponse& response)
{
    const auto auth = requireAuthenticated(request);
    requireAnyRole(auth, {ROLE_INSTRUCTOR});

    const auto body = parseJsonBody(request);
    const auto course = _database.createCourse(
        requireString(body, "title"),
        requireString(body, "description"),
        auth.id,
        normalizeCourseStatus(optionalString(body, "status").value_or(COURSE_STATUS_DRAFT)));

    Object::Ptr payload = new Object();
    payload->set("course", courseToJson(course));
    sendJson(response, HTTPResponse::HTTP_CREATED, payload);
}

void ApiRequestHandler::handleListCourses(HTTPServerRequest& request, HTTPServerResponse& response, const Poco::URI& uri)
{
    const auto auth = tryAuthenticated(request);
    const auto params = queryParams(uri);
    const auto includeAll = params.count("includeAll") > 0 && isTruthy(params.at("includeAll"));

    if (includeAll)
    {
        if (!auth)
        {
            throw ApiError(HTTPResponse::HTTP_UNAUTHORIZED, "auth_required", "Authentication is required for includeAll=true");
        }
        requireAnyRole(*auth, {ROLE_ADMIN, ROLE_INSTRUCTOR});
    }

    const auto courses = _database.listCourses(includeAll);
    Array::Ptr items = new Array();
    for (const auto& course : courses)
    {
        items->add(courseToJson(course));
    }

    Object::Ptr payload = new Object();
    payload->set("count", static_cast<int>(courses.size()));
    payload->set("items", items);
    sendJson(response, HTTPResponse::HTTP_OK, payload);
}

void ApiRequestHandler::handlePatchCourse(HTTPServerRequest& request, HTTPServerResponse& response, int courseId)
{
    const auto auth = requireAuthenticated(request);
    auto course = _database.findCourseById(courseId);
    if (!course)
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    if (auth.role != ROLE_ADMIN)
    {
        requireAnyRole(auth, {ROLE_INSTRUCTOR});
        if (course->authorId != auth.id)
        {
            throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "forbidden", "Only course author can update the course");
        }
    }

    const auto body = parseJsonBody(request);
    const auto title = optionalString(body, "title");
    const auto description = optionalString(body, "description");
    std::optional<std::string> status;
    if (body->has("status"))
    {
        status = normalizeCourseStatus(body->get("status").convert<std::string>());
    }

    if (!title && !description && !status)
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", "At least one field must be provided for PATCH");
    }
    if (title && title->empty())
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", "title must not be empty");
    }
    if (description && description->empty())
    {
        throw ApiError(HTTPResponse::HTTP_BAD_REQUEST, "validation_error", "description must not be empty");
    }

    const auto updated = _database.updateCourse(courseId, title, description, status);

    Object::Ptr payload = new Object();
    payload->set("course", courseToJson(updated));
    sendJson(response, HTTPResponse::HTTP_OK, payload);
}

void ApiRequestHandler::handleAddLesson(HTTPServerRequest& request, HTTPServerResponse& response, int courseId)
{
    const auto auth = requireAuthenticated(request);
    requireAnyRole(auth, {ROLE_INSTRUCTOR});

    const auto course = _database.findCourseById(courseId);
    if (!course)
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    if (!_database.isCourseOwnedBy(courseId, auth.id))
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "forbidden", "Only course author can add lessons");
    }

    const auto body = parseJsonBody(request);
    const auto lesson = _database.addLesson(
        courseId,
        requireString(body, "title"),
        requireString(body, "description"),
        requireInt(body, "position"));

    Object::Ptr payload = new Object();
    payload->set("lesson", lessonToJson(lesson));
    sendJson(response, HTTPResponse::HTTP_CREATED, payload);
}

void ApiRequestHandler::handleListLessons(HTTPServerRequest& request, HTTPServerResponse& response, int courseId)
{
    const auto course = _database.findCourseById(courseId);
    if (!course)
    {
        throw ApiError(HTTPResponse::HTTP_NOT_FOUND, "course_not_found", "Course not found");
    }

    if (course->status != COURSE_STATUS_PUBLISHED)
    {
        const auto auth = requireAuthenticated(request);
        const auto isOwner = auth.role == ROLE_INSTRUCTOR && course->authorId == auth.id;
        const auto isAdmin = auth.role == ROLE_ADMIN;

        if (!isOwner && !isAdmin)
        {
            throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "forbidden", "Draft or archived course lessons are available only to author or admin");
        }
    }

    const auto lessons = _database.listLessons(courseId);
    Array::Ptr items = new Array();
    for (const auto& lesson : lessons)
    {
        items->add(lessonToJson(lesson));
    }

    Object::Ptr payload = new Object();
    payload->set("course", courseToJson(*course));
    payload->set("count", static_cast<int>(lessons.size()));
    payload->set("items", items);
    sendJson(response, HTTPResponse::HTTP_OK, payload);
}

void ApiRequestHandler::handleEnroll(HTTPServerRequest& request, HTTPServerResponse& response, int courseId)
{
    const auto auth = requireAuthenticated(request);
    requireAnyRole(auth, {ROLE_STUDENT});

    const auto enrollment = _database.enrollUser(auth.id, courseId);

    Object::Ptr payload = new Object();
    payload->set("enrollment", enrollmentToJson(enrollment));
    sendJson(response, HTTPResponse::HTTP_CREATED, payload);
}

void ApiRequestHandler::handleListUserCourses(HTTPServerRequest& request, HTTPServerResponse& response, int userId)
{
    const auto auth = requireAuthenticated(request);
    ensureSelfOrAdmin(auth, userId);

    const auto courses = _database.listUserCourses(userId);
    Array::Ptr items = new Array();
    for (const auto& course : courses)
    {
        items->add(userCourseToJson(course));
    }

    Object::Ptr payload = new Object();
    payload->set("count", static_cast<int>(courses.size()));
    payload->set("items", items);
    sendJson(response, HTTPResponse::HTTP_OK, payload);
}

void ApiRequestHandler::handleCompleteLesson(HTTPServerRequest& request, HTTPServerResponse& response, int userId, int lessonId)
{
    const auto auth = requireAuthenticated(request);
    ensureSelfOrAdmin(auth, userId);

    if (auth.role != ROLE_STUDENT && auth.role != ROLE_ADMIN)
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "forbidden", "Only student or admin can mark lesson completion");
    }

    const auto completion = _database.markLessonCompleted(userId, lessonId);

    Object::Ptr payload = new Object();
    payload->set("completion", completionToJson(completion));
    sendJson(response, completion.alreadyCompleted ? HTTPResponse::HTTP_OK : HTTPResponse::HTTP_CREATED, payload);
}

std::optional<AuthContext> ApiRequestHandler::tryAuthenticated(HTTPServerRequest& request)
{
    const auto header = request.get("Authorization", "");
    if (trim(header).empty())
    {
        return std::nullopt;
    }

    const std::string prefix = "Bearer ";
    if (header.rfind(prefix, 0) != 0)
    {
        throw ApiError(HTTPResponse::HTTP_UNAUTHORIZED, "invalid_token", "Authorization header must use Bearer token");
    }

    const auto session = _database.findSessionByToken(header.substr(prefix.size()));
    if (!session)
    {
        throw ApiError(HTTPResponse::HTTP_UNAUTHORIZED, "invalid_token", "Session token is invalid");
    }
    if (session->status != USER_STATUS_ACTIVE)
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "user_blocked", "Blocked users cannot access protected endpoints");
    }

    return session;
}

AuthContext ApiRequestHandler::requireAuthenticated(HTTPServerRequest& request)
{
    const auto auth = tryAuthenticated(request);
    if (!auth)
    {
        throw ApiError(HTTPResponse::HTTP_UNAUTHORIZED, "auth_required", "Authentication is required");
    }
    return *auth;
}

void ApiRequestHandler::requireAnyRole(const AuthContext& auth, const std::initializer_list<std::string>& roles)
{
    const auto matched = std::any_of(roles.begin(), roles.end(), [&auth](const std::string& role) { return auth.role == role; });
    if (!matched)
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "forbidden", "Current user does not have required role");
    }
}

void ApiRequestHandler::ensureSelfOrAdmin(const AuthContext& auth, int userId)
{
    if (auth.role == ROLE_ADMIN)
    {
        return;
    }

    if (auth.id != userId)
    {
        throw ApiError(HTTPResponse::HTTP_FORBIDDEN, "forbidden", "Access is allowed only for the owner or admin");
    }
}

ApiRequestHandlerFactory::ApiRequestHandlerFactory(Database& database): _database(database)
{
}

HTTPRequestHandler* ApiRequestHandlerFactory::createRequestHandler(const HTTPServerRequest&)
{
    return new ApiRequestHandler(_database);
}

} // namespace lms
