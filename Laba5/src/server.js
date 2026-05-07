const crypto = require("crypto");
const express = require("express");
const { MongoClient, ObjectId } = require("mongodb");
const { createClient } = require("redis");

const PORT = Number(process.env.PORT || 8080);
const MONGO_URL = process.env.MONGO_URL || "mongodb://127.0.0.1:27017/lms_mongo";
const REDIS_URL = process.env.REDIS_URL || "redis://127.0.0.1:6379";
const CACHE_NAMESPACE = "lms:lab5";
const CACHE_TTL = {
  userByLogin: Number(process.env.USER_CACHE_TTL_SECONDS || 300),
  courses: Number(process.env.COURSES_CACHE_TTL_SECONDS || 60),
  lessons: Number(process.env.LESSONS_CACHE_TTL_SECONDS || 120),
  userCourses: Number(process.env.USER_COURSES_CACHE_TTL_SECONDS || 45)
};
const COMPLETION_RATE_LIMIT = Number(process.env.COMPLETION_RATE_LIMIT || 20);
const COMPLETION_RATE_WINDOW_MS = Number(process.env.COMPLETION_RATE_WINDOW_MS || 60_000);

const app = express();
app.use(express.json());

let database;
let cacheClient;
const memoryCache = new Map();
const memoryBuckets = new Map();

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

function normalizeObjectId(value) {
  return value instanceof ObjectId ? value : new ObjectId(value);
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

function cacheKey(...parts) {
  return [CACHE_NAMESPACE, ...parts.map((part) => String(part).replace(/\s+/g, "_"))].join(":");
}

function redisReady() {
  return cacheClient && cacheClient.isReady;
}

async function connectCache() {
  cacheClient = createClient({
    url: REDIS_URL,
    socket: {
      connectTimeout: 500,
      reconnectStrategy: false
    }
  });
  cacheClient.on("error", (error) => {
    console.warn(`Redis cache error: ${error.message}`);
  });

  try {
    await cacheClient.connect();
    console.log(`Redis cache connected: ${REDIS_URL}`);
  } catch (error) {
    console.warn(`Redis cache is unavailable, using local memory cache: ${error.message}`);
  }
}

async function getCachedJson(key) {
  if (redisReady()) {
    const raw = await cacheClient.get(key);
    return raw ? JSON.parse(raw) : null;
  }

  const cached = memoryCache.get(key);
  if (!cached) {
    return null;
  }
  if (cached.expiresAt <= Date.now()) {
    memoryCache.delete(key);
    return null;
  }
  return cached.value;
}

async function setCachedJson(key, value, ttlSeconds) {
  if (redisReady()) {
    await cacheClient.set(key, JSON.stringify(value), { EX: ttlSeconds });
    return;
  }

  memoryCache.set(key, {
    value,
    expiresAt: Date.now() + ttlSeconds * 1000
  });
}

async function deleteCachePrefix(prefix) {
  const fullPrefix = cacheKey(prefix);

  if (redisReady()) {
    const keys = [];
    for await (const key of cacheClient.scanIterator({ MATCH: `${fullPrefix}*`, COUNT: 100 })) {
      keys.push(key);
    }
    if (keys.length > 0) {
      await cacheClient.del(keys);
    }
  }

  for (const key of memoryCache.keys()) {
    if (key.startsWith(fullPrefix)) {
      memoryCache.delete(key);
    }
  }
}

function sendCachedResponse(response, payload, cacheStatus) {
  response.set("X-Cache", cacheStatus);
  response.json(payload);
}

async function getBucket(key) {
  if (redisReady()) {
    const raw = await cacheClient.get(key);
    return raw ? JSON.parse(raw) : null;
  }

  return memoryBuckets.get(key) || null;
}

async function setBucket(key, bucket, ttlSeconds) {
  if (redisReady()) {
    await cacheClient.set(key, JSON.stringify(bucket), { EX: ttlSeconds });
    return;
  }

  memoryBuckets.set(key, bucket);
  setTimeout(() => memoryBuckets.delete(key), ttlSeconds * 1000).unref();
}

function clientIp(request) {
  return request.headers["x-forwarded-for"]?.split(",")[0]?.trim() || request.ip || request.socket.remoteAddress || "unknown";
}

function tokenBucketRateLimit({ name, limit, windowMs }) {
  const refillPerMs = limit / windowMs;
  const ttlSeconds = Math.ceil((windowMs * 2) / 1000);

  return async (request, response, next) => {
    try {
      const now = Date.now();
      const key = cacheKey("rate", name, clientIp(request));
      const previous = await getBucket(key);
      const previousTokens = previous ? Number(previous.tokens) : limit;
      const previousUpdatedAt = previous ? Number(previous.updatedAt) : now;
      const tokens = Math.min(limit, previousTokens + Math.max(0, now - previousUpdatedAt) * refillPerMs);

      if (tokens < 1) {
        const retryMs = Math.ceil((1 - tokens) / refillPerMs);
        const resetAt = Math.ceil((now + retryMs) / 1000);

        response.set("X-RateLimit-Limit", String(limit));
        response.set("X-RateLimit-Remaining", "0");
        response.set("X-RateLimit-Reset", String(resetAt));
        response.set("Retry-After", String(Math.ceil(retryMs / 1000)));
        await setBucket(key, { tokens, updatedAt: now }, ttlSeconds);

        response.status(429).json({
          error: {
            code: "rate_limit_exceeded",
            message: "too many requests"
          }
        });
        return;
      }

      const remainingTokens = tokens - 1;
      const fullResetMs = Math.ceil((limit - remainingTokens) / refillPerMs);
      const resetAt = Math.ceil((now + fullResetMs) / 1000);

      response.set("X-RateLimit-Limit", String(limit));
      response.set("X-RateLimit-Remaining", String(Math.floor(remainingTokens)));
      response.set("X-RateLimit-Reset", String(resetAt));
      await setBucket(key, { tokens: remainingTokens, updatedAt: now }, ttlSeconds);

      next();
    } catch (error) {
      next(error);
    }
  };
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

function publicCompletion(completion) {
  return {
    userId: completion.userId.toString(),
    lessonId: completion.lessonId.toString(),
    completedAt: completion.completedAt,
    alreadyCompleted: completion.alreadyCompleted,
    enrollmentStatus: completion.enrollmentStatus,
    progressPercent: completion.progressPercent
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
  response.json({
    status: "ok",
    service: "lms-performance-api",
    storage: "mongodb",
    cache: redisReady() ? "redis" : "memory"
  });
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
    await deleteCachePrefix(`users:login:${login}`);
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

    const key = cacheKey("users", "login", login);
    const cached = await getCachedJson(key);
    if (cached) {
      sendCachedResponse(response, cached, "HIT");
      return;
    }

    const user = await database.collection("users").findOne({ login });
    if (!user) {
      const error = new Error("user not found");
      error.status = 404;
      throw error;
    }

    const payload = { user: publicUser(user) };
    await setCachedJson(key, payload, CACHE_TTL.userByLogin);
    sendCachedResponse(response, payload, "MISS");
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
    await deleteCachePrefix("courses:list");
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

    const key = cacheKey("courses", "list", filter.status || "all");
    const cached = await getCachedJson(key);
    if (cached) {
      sendCachedResponse(response, cached, "HIT");
      return;
    }

    const courses = await database.collection("courses").find(filter).sort({ createdAt: -1 }).toArray();
    const payload = { count: courses.length, items: courses.map(publicCourse) };
    await setCachedJson(key, payload, CACHE_TTL.courses);
    sendCachedResponse(response, payload, "MISS");
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
    await deleteCachePrefix(`lessons:course:${courseId.toString()}`);
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
    const key = cacheKey("lessons", "course", courseId.toString());
    const cached = await getCachedJson(key);
    if (cached) {
      sendCachedResponse(response, cached, "HIT");
      return;
    }

    const lessons = await database.collection("lessons").find({ courseId }).sort({ position: 1 }).toArray();
    const payload = { count: lessons.length, items: lessons.map(publicLesson) };
    await setCachedJson(key, payload, CACHE_TTL.lessons);
    sendCachedResponse(response, payload, "MISS");
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
    await deleteCachePrefix(`user:courses:${userId.toString()}`);
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
    const key = cacheKey("user", "courses", userId.toString());
    const cached = await getCachedJson(key);
    if (cached) {
      sendCachedResponse(response, cached, "HIT");
      return;
    }

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

    const payload = { count: items.length, items };
    await setCachedJson(key, payload, CACHE_TTL.userCourses);
    sendCachedResponse(response, payload, "MISS");
  } catch (error) {
    next(error);
  }
});

app.post(
  "/users/:userId/lessons/:lessonId/completion",
  tokenBucketRateLimit({
    name: "lesson-completion",
    limit: COMPLETION_RATE_LIMIT,
    windowMs: COMPLETION_RATE_WINDOW_MS
  }),
  async (request, response, next) => {
    try {
      const userId = parseObjectId(request.params.userId, "userId");
      const lessonId = parseObjectId(request.params.lessonId, "lessonId");
      const lesson = await database.collection("lessons").findOne({ _id: lessonId });

      if (!lesson) {
        const error = new Error("lesson not found");
        error.status = 404;
        throw error;
      }

      const enrollment = await database.collection("enrollments").findOne({
        userId,
        courseId: lesson.courseId,
        status: { $ne: "CANCELLED" }
      });

      if (!enrollment) {
        const error = new Error("student must be enrolled in the course before completing lessons");
        error.status = 409;
        throw error;
      }

      const now = new Date();
      const progress = enrollment.progress || {};
      const completedLessonIds = (progress.completedLessonIds || []).map(normalizeObjectId);
      const completedAtByLesson = progress.completedAtByLesson || {};
      const alreadyCompleted = completedLessonIds.some((id) => id.equals(lessonId));

      if (!alreadyCompleted) {
        completedLessonIds.push(lessonId);
        completedAtByLesson[lessonId.toString()] = now;
      }

      const lessonCount = await database.collection("lessons").countDocuments({ courseId: lesson.courseId });
      const percent = lessonCount > 0 ? Math.round((completedLessonIds.length / lessonCount) * 100) : 0;
      const enrollmentStatus = percent >= 100 ? "COMPLETED" : "ACTIVE";

      await database.collection("enrollments").updateOne(
        { _id: enrollment._id },
        {
          $set: {
            status: enrollmentStatus,
            progress: {
              completedLessonIds,
              completedAtByLesson,
              percent
            },
            lastActivityAt: now
          }
        }
      );
      await deleteCachePrefix(`user:courses:${userId.toString()}`);

      const completion = {
        userId,
        lessonId,
        completedAt: completedAtByLesson[lessonId.toString()] || now,
        alreadyCompleted,
        enrollmentStatus,
        progressPercent: percent
      };

      response.status(alreadyCompleted ? 200 : 201).json({ completion: publicCompletion(completion) });
    } catch (error) {
      next(error);
    }
  }
);

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
  await connectCache();

  app.listen(PORT, () => {
    console.log(`LMS performance API is running on port ${PORT}`);
  });
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
