const crypto = require("crypto");
const express = require("express");
const { MongoClient, ObjectId } = require("mongodb");

const PORT = Number(process.env.PORT || 8080);
const MONGO_URL = process.env.MONGO_URL || "mongodb://127.0.0.1:27017/lms_mongo";

const app = express();
app.use(express.json());

let database;

function trimText(value) {
  return String(value ?? "").trim();
}

function requireText(body, fieldName) {
  const value = trimText(body[fieldName]);
  if (!value) {
    const error = new Error(`${fieldName} is required`);
    error.status = 400;
    throw error;
  }
  return value;
}

function parseObjectId(value, fieldName) {
  if (!ObjectId.isValid(value)) {
    const error = new Error(`${fieldName} must be a valid ObjectId`);
    error.status = 400;
    throw error;
  }
  return new ObjectId(value);
}

function hashPassword(password) {
  return `sha256:${crypto.createHash("sha256").update(password).digest("hex")}`;
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function makeSlug(title) {
  return title
    .toLowerCase()
    .replace(/[^a-zа-я0-9]+/gi, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 80);
}

function publicUser(user) {
  return {
    id: user._id.toString(),
    login: user.login,
    firstName: user.name.first,
    lastName: user.name.last,
    email: user.email,
    role: user.role,
    status: user.status,
    profile: user.profile || {},
    createdAt: user.createdAt
  };
}

function publicCourse(course) {
  return {
    id: course._id.toString(),
    slug: course.slug,
    title: course.title,
    description: course.description,
    authorId: course.authorId.toString(),
    status: course.status,
    tags: course.tags || [],
    settings: course.settings || {},
    createdAt: course.createdAt
  };
}

function publicLesson(lesson) {
  return {
    id: lesson._id.toString(),
    courseId: lesson.courseId.toString(),
    title: lesson.title,
    description: lesson.description,
    position: lesson.position,
    durationMinutes: lesson.durationMinutes,
    content: lesson.content || {},
    status: lesson.status,
    createdAt: lesson.createdAt
  };
}

function publicEnrollment(enrollment) {
  return {
    id: enrollment._id.toString(),
    userId: enrollment.userId.toString(),
    courseId: enrollment.courseId.toString(),
    status: enrollment.status,
    progress: enrollment.progress || { completedLessonIds: [], percent: 0 },
    enrolledAt: enrollment.enrolledAt
  };
}

async function ensureIndexes() {
  await database.collection("users").createIndex({ login: 1 }, { unique: true });
  await database.collection("users").createIndex({ email: 1 }, { unique: true });
  await database.collection("courses").createIndex({ slug: 1 }, { unique: true });
  await database.collection("lessons").createIndex({ courseId: 1, position: 1 }, { unique: true });
  await database.collection("enrollments").createIndex({ userId: 1, courseId: 1 }, { unique: true });
}

app.get("/health", (_request, response) => {
  response.json({ status: "ok", service: "lms-mongodb-api", storage: "mongodb" });
});

app.post("/users", async (request, response, next) => {
  try {
    const firstName = requireText(request.body, "firstName");
    const lastName = requireText(request.body, "lastName");
    const login = requireText(request.body, "login").toLowerCase();
    const email = requireText(request.body, "email").toLowerCase();
    const password = requireText(request.body, "password");
    const role = trimText(request.body.role || "STUDENT").toUpperCase();
    const status = trimText(request.body.status || "ACTIVE").toUpperCase();

    const user = {
      login,
      name: { first: firstName, last: lastName },
      email,
      passwordHash: hashPassword(password),
      role,
      status,
      profile: request.body.profile || {},
      createdAt: new Date()
    };

    const result = await database.collection("users").insertOne(user);
    response.status(201).json({ user: publicUser({ ...user, _id: result.insertedId }) });
  } catch (error) {
    if (error.code === 11000) {
      error.status = 409;
      error.message = "user with this login or email already exists";
    }
    next(error);
  }
});

app.get("/users/by-login", async (request, response, next) => {
  try {
    const login = trimText(request.query.login).toLowerCase();
    if (!login) {
      const error = new Error("login query parameter is required");
      error.status = 400;
      throw error;
    }

    const user = await database.collection("users").findOne({ login });
    if (!user) {
      const error = new Error("user not found");
      error.status = 404;
      throw error;
    }

    response.json({ user: publicUser(user) });
  } catch (error) {
    next(error);
  }
});

app.get("/users/search", async (request, response, next) => {
  try {
    const firstNameMask = trimText(request.query.firstNameMask);
    const lastNameMask = trimText(request.query.lastNameMask);
    const conditions = [];

    if (firstNameMask) {
      conditions.push({ "name.first": { $regex: escapeRegExp(firstNameMask), $options: "i" } });
    }
    if (lastNameMask) {
      conditions.push({ "name.last": { $regex: escapeRegExp(lastNameMask), $options: "i" } });
    }

    const filter = conditions.length > 0 ? { $and: conditions } : {};
    const users = await database
      .collection("users")
      .find(filter)
      .sort({ "name.last": 1, "name.first": 1 })
      .toArray();

    response.json({ count: users.length, items: users.map(publicUser) });
  } catch (error) {
    next(error);
  }
});

app.post("/courses", async (request, response, next) => {
  try {
    const title = requireText(request.body, "title");
    const authorId = parseObjectId(requireText(request.body, "authorId"), "authorId");
    const author = await database.collection("users").findOne({ _id: authorId, role: "INSTRUCTOR" });
    if (!author) {
      const error = new Error("course author must be an existing instructor");
      error.status = 400;
      throw error;
    }

    const course = {
      slug: trimText(request.body.slug) || makeSlug(title),
      title,
      description: requireText(request.body, "description"),
      authorId,
      status: trimText(request.body.status || "DRAFT").toUpperCase(),
      tags: Array.isArray(request.body.tags) ? request.body.tags : [],
      settings: request.body.settings || { language: "ru", difficulty: "beginner", certificate: false },
      createdAt: new Date()
    };

    const result = await database.collection("courses").insertOne(course);
    response.status(201).json({ course: publicCourse({ ...course, _id: result.insertedId }) });
  } catch (error) {
    if (error.code === 11000) {
      error.status = 409;
      error.message = "course slug already exists";
    }
    next(error);
  }
});

app.get("/courses", async (request, response, next) => {
  try {
    const filter = {};
    if (request.query.status) {
      filter.status = trimText(request.query.status).toUpperCase();
    }

    const courses = await database.collection("courses").find(filter).sort({ createdAt: -1 }).toArray();
    response.json({ count: courses.length, items: courses.map(publicCourse) });
  } catch (error) {
    next(error);
  }
});

app.post("/courses/:courseId/lessons", async (request, response, next) => {
  try {
    const courseId = parseObjectId(request.params.courseId, "courseId");
    const course = await database.collection("courses").findOne({ _id: courseId });
    if (!course) {
      const error = new Error("course not found");
      error.status = 404;
      throw error;
    }

    const lesson = {
      courseId,
      title: requireText(request.body, "title"),
      description: requireText(request.body, "description"),
      position: Number(request.body.position),
      durationMinutes: Number(request.body.durationMinutes || 30),
      content: request.body.content || { blocks: [] },
      status: trimText(request.body.status || "DRAFT").toUpperCase(),
      createdAt: new Date()
    };

    if (!Number.isInteger(lesson.position) || lesson.position < 1) {
      const error = new Error("position must be a positive integer");
      error.status = 400;
      throw error;
    }

    const result = await database.collection("lessons").insertOne(lesson);
    response.status(201).json({ lesson: publicLesson({ ...lesson, _id: result.insertedId }) });
  } catch (error) {
    if (error.code === 11000) {
      error.status = 409;
      error.message = "lesson position already exists in this course";
    }
    next(error);
  }
});

app.get("/courses/:courseId/lessons", async (request, response, next) => {
  try {
    const courseId = parseObjectId(request.params.courseId, "courseId");
    const lessons = await database.collection("lessons").find({ courseId }).sort({ position: 1 }).toArray();
    response.json({ count: lessons.length, items: lessons.map(publicLesson) });
  } catch (error) {
    next(error);
  }
});

app.post("/courses/:courseId/enrollments", async (request, response, next) => {
  try {
    const courseId = parseObjectId(request.params.courseId, "courseId");
    const userId = parseObjectId(requireText(request.body, "userId"), "userId");

    const [course, user] = await Promise.all([
      database.collection("courses").findOne({ _id: courseId, status: { $ne: "ARCHIVED" } }),
      database.collection("users").findOne({ _id: userId, status: "ACTIVE" })
    ]);

    if (!course) {
      const error = new Error("course not found or archived");
      error.status = 404;
      throw error;
    }
    if (!user) {
      const error = new Error("active user not found");
      error.status = 404;
      throw error;
    }

    const enrollment = {
      userId,
      courseId,
      status: "ACTIVE",
      progress: { completedLessonIds: [], percent: 0 },
      enrolledAt: new Date()
    };

    const result = await database.collection("enrollments").insertOne(enrollment);
    response.status(201).json({ enrollment: publicEnrollment({ ...enrollment, _id: result.insertedId }) });
  } catch (error) {
    if (error.code === 11000) {
      error.status = 409;
      error.message = "user is already enrolled in this course";
    }
    next(error);
  }
});

app.get("/users/:userId/courses", async (request, response, next) => {
  try {
    const userId = parseObjectId(request.params.userId, "userId");
    const items = await database
      .collection("enrollments")
      .aggregate([
        { $match: { userId } },
        {
          $lookup: {
            from: "courses",
            localField: "courseId",
            foreignField: "_id",
            as: "course"
          }
        },
        { $unwind: "$course" },
        { $sort: { enrolledAt: -1 } },
        {
          $project: {
            _id: 0,
            enrollmentId: { $toString: "$_id" },
            status: "$status",
            progress: "$progress",
            enrolledAt: "$enrolledAt",
            course: {
              id: { $toString: "$course._id" },
              slug: "$course.slug",
              title: "$course.title",
              description: "$course.description",
              status: "$course.status",
              tags: "$course.tags"
            }
          }
        }
      ])
      .toArray();

    response.json({ count: items.length, items });
  } catch (error) {
    next(error);
  }
});

app.use((error, _request, response, _next) => {
  if (error.code === 121) {
    error.status = 400;
    error.message = "document failed MongoDB schema validation";
  }

  const status = error.status || 500;
  response.status(status).json({
    error: {
      code: status >= 500 ? "internal_error" : "request_error",
      message: error.message
    }
  });
});

async function main() {
  const client = new MongoClient(MONGO_URL);
  await client.connect();
  database = client.db();
  await ensureIndexes();

  app.listen(PORT, () => {
    console.log(`LMS MongoDB API is running on port ${PORT}`);
  });
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
