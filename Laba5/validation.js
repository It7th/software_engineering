const dbName = "lms_mongo";
const database = db.getSiblingDB(dbName);

database.dropDatabase();

database.createCollection("users", {
  validator: {
    $jsonSchema: {
      bsonType: "object",
      required: ["login", "name", "email", "passwordHash", "role", "status", "createdAt"],
      additionalProperties: true,
      properties: {
        login: {
          bsonType: "string",
          pattern: "^[a-zA-Z][a-zA-Z0-9_.-]{2,31}$",
          description: "login должен начинаться с буквы и быть длиной 3-32 символа"
        },
        name: {
          bsonType: "object",
          required: ["first", "last"],
          properties: {
            first: { bsonType: "string", minLength: 2, maxLength: 60 },
            last: { bsonType: "string", minLength: 2, maxLength: 60 }
          }
        },
        email: {
          bsonType: "string",
          pattern: "^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$"
        },
        passwordHash: {
          bsonType: "string",
          minLength: 20
        },
        role: {
          enum: ["ADMIN", "INSTRUCTOR", "STUDENT"]
        },
        status: {
          enum: ["ACTIVE", "BLOCKED"]
        },
        profile: {
          bsonType: "object"
        },
        createdAt: {
          bsonType: "date"
        }
      }
    }
  },
  validationLevel: "strict",
  validationAction: "error"
});

database.createCollection("courses");
database.createCollection("lessons");
database.createCollection("enrollments");

database.users.createIndex({ login: 1 }, { unique: true });
database.users.createIndex({ email: 1 }, { unique: true });
database.users.createIndex({ "name.first": 1, "name.last": 1 });

database.courses.createIndex({ slug: 1 }, { unique: true });
database.courses.createIndex({ status: 1, createdAt: -1 });
database.courses.createIndex({ authorId: 1 });

database.lessons.createIndex({ courseId: 1, position: 1 }, { unique: true });
database.lessons.createIndex({ courseId: 1, status: 1 });

database.enrollments.createIndex({ userId: 1, courseId: 1 }, { unique: true });
database.enrollments.createIndex({ courseId: 1, status: 1 });

print("MongoDB collections, validation rules and indexes are ready.");

try {
  database.users.insertOne({
    login: "1",
    name: { first: "A", last: "B" },
    email: "broken-email",
    passwordHash: "short",
    role: "GUEST",
    status: "ACTIVE",
    createdAt: new Date()
  });
} catch (error) {
  print("Validation check works: invalid user was rejected.");
}
