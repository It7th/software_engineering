#!/usr/bin/env python3
import json
import os
import sys
import uuid
from urllib import error, parse, request


BASE_URL = os.environ.get("BASE_URL", "http://localhost:8080").rstrip("/")


def call_api(method, path, body=None, token=None, expected=(200,)):
    headers = {"Content-Type": "application/json"}
    data = None

    if token:
        headers["Authorization"] = f"Bearer {token}"

    if body is not None:
        data = json.dumps(body).encode("utf-8")

    http_request = request.Request(f"{BASE_URL}{path}", data=data, headers=headers, method=method)

    try:
        with request.urlopen(http_request, timeout=10) as response:
            raw = response.read().decode("utf-8")
            payload = json.loads(raw) if raw else {}
            status = response.status
    except error.HTTPError as exc:
        raw = exc.read().decode("utf-8")
        payload = json.loads(raw) if raw else {}
        status = exc.code
    except error.URLError as exc:
        raise RuntimeError(f"Cannot reach API at {BASE_URL}: {exc}") from exc

    if status not in expected:
        raise AssertionError(
            f"{method} {path} returned {status}, expected {expected}. "
            f"Payload: {json.dumps(payload, ensure_ascii=False)}"
        )

    return status, payload


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def step(title):
    print(f"[check] {title}")


def login(login_value, password):
    _, payload = call_api(
        "POST",
        "/auth/login",
        {"login": login_value, "password": password},
    )
    return payload["token"], payload["user"]


def register_student(login_value, email):
    _, payload = call_api(
        "POST",
        "/auth/register",
        {
            "login": login_value,
            "firstName": "Ivan",
            "lastName": "Petrov",
            "email": email,
            "password": "secret123",
        },
        expected=(201,),
    )
    return payload["user"]


def create_course(token, title, description, status="DRAFT"):
    _, payload = call_api(
        "POST",
        "/courses",
        {
            "title": title,
            "description": description,
            "status": status,
        },
        token=token,
        expected=(201,),
    )
    return payload["course"]


def add_lesson(token, course_id, title, description, position):
    _, payload = call_api(
        "POST",
        f"/courses/{course_id}/lessons",
        {
            "title": title,
            "description": description,
            "position": position,
        },
        token=token,
        expected=(201,),
    )
    return payload["lesson"]


def main():
    suffix = uuid.uuid4().hex[:8]
    student_login = f"student_{suffix}"
    student_email = f"{student_login}@example.com"

    print(f"Using API base URL: {BASE_URL}")

    step("server responds to /health")
    status, _ = call_api("GET", "/health")
    require(status == 200, "Health check failed")

    step("system users can log in")
    admin_token, _ = login("admin", "admin123")
    instructor_token, _ = login("instructor", "instructor123")

    step("student can register and then log in")
    student = register_student(student_login, student_email)
    student_id = student["id"]
    student_token, _ = login(student_login, "secret123")

    step("admin can find the new user")
    encoded_login = parse.quote(student_login)
    _, by_login = call_api("GET", f"/users/by-login?login={encoded_login}", token=admin_token)
    require(by_login["user"]["id"] == student_id, "User lookup by login returned wrong id")

    _, by_mask = call_api(
        "GET",
        "/users/search?firstNameMask=Iv&lastNameMask=Pe",
        token=admin_token,
    )
    require(by_mask["count"] >= 1, "Search by name mask should not be empty")

    step("instructor creates a draft course with two lessons")
    course = create_course(
        instructor_token,
        f"REST API Basics {suffix}",
        "Introduction to REST services and HTTP.",
        status="DRAFT",
    )
    course_id = course["id"]

    first_lesson = add_lesson(
        instructor_token,
        course_id,
        "HTTP Methods",
        "GET, POST, PUT, PATCH and DELETE.",
        1,
    )
    second_lesson = add_lesson(
        instructor_token,
        course_id,
        "Status Codes",
        "2xx, 4xx and 5xx families.",
        2,
    )

    step("draft lessons are not publicly available yet")
    draft_status, _ = call_api("GET", f"/courses/{course_id}/lessons", expected=(401,))
    require(draft_status == 401, "Draft lessons should require authentication")

    step("after publish the course appears in the public catalog")
    call_api(
        "PATCH",
        f"/courses/{course_id}",
        {"status": "PUBLISHED"},
        token=instructor_token,
    )

    _, catalog = call_api("GET", "/courses")
    require(any(item["id"] == course_id for item in catalog["items"]), "Published course is missing from catalog")

    _, lessons = call_api("GET", f"/courses/{course_id}/lessons")
    require(lessons["count"] == 2, "Course should contain two lessons")

    step("student can enroll once, but not twice")
    _, enrollment = call_api(
        "POST",
        f"/courses/{course_id}/enrollments",
        token=student_token,
        expected=(201,),
    )
    require(enrollment["enrollment"]["courseId"] == course_id, "Enrollment returned wrong course id")

    call_api(
        "POST",
        f"/courses/{course_id}/enrollments",
        token=student_token,
        expected=(409,),
    )

    _, student_courses = call_api("GET", f"/users/{student_id}/courses", token=student_token)
    require(student_courses["count"] == 1, "Student should have one course after enrollment")

    step("lesson completion is idempotent and updates enrollment status")
    _, completion_1 = call_api(
        "POST",
        f"/users/{student_id}/lessons/{first_lesson['id']}/completion",
        token=student_token,
        expected=(201,),
    )
    require(
        completion_1["completion"]["alreadyCompleted"] is False,
        "First completion should not be marked as repeated",
    )

    _, completion_repeat = call_api(
        "POST",
        f"/users/{student_id}/lessons/{first_lesson['id']}/completion",
        token=student_token,
        expected=(200,),
    )
    require(
        completion_repeat["completion"]["alreadyCompleted"] is True,
        "Second completion should be idempotent",
    )

    _, completion_2 = call_api(
        "POST",
        f"/users/{student_id}/lessons/{second_lesson['id']}/completion",
        token=student_token,
        expected=(201,),
    )
    require(
        completion_2["completion"]["enrollmentStatus"] == "COMPLETED",
        "Enrollment should switch to COMPLETED after the last lesson",
    )

    step("a couple of negative cases still behave as expected")
    call_api(
        "POST",
        "/courses",
        {
            "title": "Forbidden course",
            "description": "Student cannot create this",
        },
        token=student_token,
        expected=(403,),
    )

    call_api(
        "POST",
        "/auth/login",
        {"login": "admin", "password": "wrong-password"},
        expected=(401,),
    )

    print("Smoke test passed.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"Smoke test failed: {exc}", file=sys.stderr)
        sys.exit(1)
