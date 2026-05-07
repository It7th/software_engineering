const database = db.getSiblingDB("lms_mongo");

const userId = ObjectId("660000000000000000000004");
const courseId = ObjectId("661000000000000000000001");

database.users.deleteOne({ login: "new.student" });
database.courses.deleteOne({ slug: "node-api-with-mongodb" });

// Create: новый пользователь
database.users.insertOne({
  login: "new.student",
  name: { first: "Денис", last: "Ильин" },
  email: "denis.ilin@example.com",
  passwordHash: "sha256:new-student-password-hash",
  role: "STUDENT",
  status: "ACTIVE",
  profile: { city: "Рязань", interests: ["mongodb"] },
  createdAt: new Date()
});

// Create: новый курс
database.courses.insertOne({
  slug: "node-api-with-mongodb",
  title: "Node.js API с MongoDB",
  description: "Небольшой курс про API и работу с документами.",
  authorId: ObjectId("660000000000000000000002"),
  status: "DRAFT",
  tags: ["nodejs", "mongodb"],
  settings: { language: "ru", difficulty: "beginner", certificate: false },
  createdAt: new Date()
});

// Read: поиск пользователя по логину, оператор $eq
database.users.findOne({ login: { $eq: "petrov" } });

// Read: поиск по маске имени и фамилии, $and + регулярные выражения
database.users.find({
  $and: [
    { "name.first": { $regex: "па", $options: "i" } },
    { "name.last": { $regex: "пет", $options: "i" } }
  ]
});

// Read: опубликованные или черновые курсы, $in
database.courses.find({
  status: { $in: ["PUBLISHED", "DRAFT"] }
}).sort({ createdAt: -1 });

// Read: уроки курса длиннее 30 минут, $gt
database.lessons.find({
  courseId: { $eq: courseId },
  durationMinutes: { $gt: 30 }
}).sort({ position: 1 });

// Read: активные пользователи, кроме преподавателей, $ne
database.users.find({
  status: "ACTIVE",
  role: { $ne: "INSTRUCTOR" }
});

// Read: курсы с сертификатом или по backend-тегам, $or
database.courses.find({
  $or: [
    { "settings.certificate": true },
    { tags: { $in: ["backend", "api"] } }
  ]
});

// Read: короткие уроки, $lt
database.lessons.find({
  durationMinutes: { $lt: 30 }
});

// Update: поменять статус курса
database.courses.updateOne(
  { slug: "node-api-with-mongodb" },
  { $set: { status: "PUBLISHED", updatedAt: new Date() } }
);

// Update: добавить тег к курсу без дублей, $addToSet
database.courses.updateOne(
  { _id: courseId },
  { $addToSet: { tags: "schema-design" } }
);

// Update: добавить пройденный урок в enrollment, $push
database.enrollments.updateOne(
  { userId, courseId },
  {
    $push: { "progress.completedLessonIds": ObjectId("662000000000000000000002") },
    $set: { "progress.percent": 100, status: "COMPLETED" }
  }
);

// Update: убрать урок из прогресса, $pull
database.enrollments.updateOne(
  { userId, courseId },
  {
    $pull: { "progress.completedLessonIds": ObjectId("662000000000000000000002") },
    $set: { "progress.percent": 50, status: "ACTIVE" }
  }
);

// Delete: удалить черновой урок
database.lessons.deleteOne({
  courseId,
  position: 99,
  status: "DRAFT"
});

// Delete: отменить запись пользователя на курс
database.enrollments.deleteOne({
  userId: ObjectId("660000000000000000000008"),
  courseId: ObjectId("661000000000000000000005"),
  status: "CANCELLED"
});

// Aggregation: сколько активных студентов на каждом курсе
database.enrollments.aggregate([
  { $match: { status: { $eq: "ACTIVE" } } },
  {
    $group: {
      _id: "$courseId",
      studentsCount: { $sum: 1 },
      avgProgress: { $avg: "$progress.percent" }
    }
  },
  {
    $lookup: {
      from: "courses",
      localField: "_id",
      foreignField: "_id",
      as: "course"
    }
  },
  { $unwind: "$course" },
  {
    $project: {
      _id: 0,
      courseId: { $toString: "$_id" },
      title: "$course.title",
      studentsCount: 1,
      avgProgress: { $round: ["$avgProgress", 1] }
    }
  },
  { $sort: { studentsCount: -1, title: 1 } }
]);
