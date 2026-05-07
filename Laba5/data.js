const database = db.getSiblingDB("lms_mongo");

const ids = {
  admin: ObjectId("660000000000000000000001"),
  instructor1: ObjectId("660000000000000000000002"),
  instructor2: ObjectId("660000000000000000000003"),
  student1: ObjectId("660000000000000000000004"),
  student2: ObjectId("660000000000000000000005"),
  student3: ObjectId("660000000000000000000006"),
  student4: ObjectId("660000000000000000000007"),
  student5: ObjectId("660000000000000000000008"),
  student6: ObjectId("660000000000000000000009"),
  student7: ObjectId("660000000000000000000010"),

  course1: ObjectId("661000000000000000000001"),
  course2: ObjectId("661000000000000000000002"),
  course3: ObjectId("661000000000000000000003"),
  course4: ObjectId("661000000000000000000004"),
  course5: ObjectId("661000000000000000000005"),
  course6: ObjectId("661000000000000000000006"),
  course7: ObjectId("661000000000000000000007"),
  course8: ObjectId("661000000000000000000008"),
  course9: ObjectId("661000000000000000000009"),
  course10: ObjectId("661000000000000000000010")
};

database.users.deleteMany({});
database.courses.deleteMany({});
database.lessons.deleteMany({});
database.enrollments.deleteMany({});

database.users.insertMany([
  {
    _id: ids.admin,
    login: "admin",
    name: { first: "Анна", last: "Орлова" },
    email: "admin@lms.local",
    passwordHash: "sha256:admin123-just-for-lab",
    role: "ADMIN",
    status: "ACTIVE",
    profile: { city: "Москва", about: "Администратор учебной платформы" },
    createdAt: new Date("2026-02-01T09:00:00Z")
  },
  {
    _id: ids.instructor1,
    login: "ivan.teacher",
    name: { first: "Иван", last: "Соколов" },
    email: "ivan.sokolov@lms.local",
    passwordHash: "sha256:teacher123-just-for-lab",
    role: "INSTRUCTOR",
    status: "ACTIVE",
    profile: { city: "Казань", about: "Ведет backend и базы данных" },
    createdAt: new Date("2026-02-03T12:20:00Z")
  },
  {
    _id: ids.instructor2,
    login: "maria.ui",
    name: { first: "Мария", last: "Белова" },
    email: "maria.belova@lms.local",
    passwordHash: "sha256:teacher456-just-for-lab",
    role: "INSTRUCTOR",
    status: "ACTIVE",
    profile: { city: "Санкт-Петербург", about: "UI, UX и продуктовая аналитика" },
    createdAt: new Date("2026-02-05T08:30:00Z")
  },
  {
    _id: ids.student1,
    login: "petrov",
    name: { first: "Павел", last: "Петров" },
    email: "pavel.petrov@example.com",
    passwordHash: "sha256:student001-just-for-lab",
    role: "STUDENT",
    status: "ACTIVE",
    profile: { city: "Москва", interests: ["backend", "api"] },
    createdAt: new Date("2026-02-10T10:00:00Z")
  },
  {
    _id: ids.student2,
    login: "alisa.k",
    name: { first: "Алиса", last: "Крылова" },
    email: "alisa.krylova@example.com",
    passwordHash: "sha256:student002-just-for-lab",
    role: "STUDENT",
    status: "ACTIVE",
    profile: { city: "Тула", interests: ["design", "frontend"] },
    createdAt: new Date("2026-02-11T10:00:00Z")
  },
  {
    _id: ids.student3,
    login: "sergey_m",
    name: { first: "Сергей", last: "Морозов" },
    email: "sergey.morozov@example.com",
    passwordHash: "sha256:student003-just-for-lab",
    role: "STUDENT",
    status: "ACTIVE",
    profile: { city: "Самара", interests: ["testing", "devops"] },
    createdAt: new Date("2026-02-12T10:00:00Z")
  },
  {
    _id: ids.student4,
    login: "nina.data",
    name: { first: "Нина", last: "Федорова" },
    email: "nina.fedorova@example.com",
    passwordHash: "sha256:student004-just-for-lab",
    role: "STUDENT",
    status: "ACTIVE",
    profile: { city: "Пермь", interests: ["analytics", "sql"] },
    createdAt: new Date("2026-02-13T10:00:00Z")
  },
  {
    _id: ids.student5,
    login: "timur.dev",
    name: { first: "Тимур", last: "Галеев" },
    email: "timur.galeev@example.com",
    passwordHash: "sha256:student005-just-for-lab",
    role: "STUDENT",
    status: "ACTIVE",
    profile: { city: "Уфа", interests: ["architecture", "mongodb"] },
    createdAt: new Date("2026-02-14T10:00:00Z")
  },
  {
    _id: ids.student6,
    login: "vera.qa",
    name: { first: "Вера", last: "Лапина" },
    email: "vera.lapina@example.com",
    passwordHash: "sha256:student006-just-for-lab",
    role: "STUDENT",
    status: "BLOCKED",
    profile: { city: "Омск", interests: ["qa", "automation"] },
    createdAt: new Date("2026-02-15T10:00:00Z")
  },
  {
    _id: ids.student7,
    login: "oleg.coder",
    name: { first: "Олег", last: "Никитин" },
    email: "oleg.nikitin@example.com",
    passwordHash: "sha256:student007-just-for-lab",
    role: "STUDENT",
    status: "ACTIVE",
    profile: { city: "Воронеж", interests: ["c++", "systems"] },
    createdAt: new Date("2026-02-16T10:00:00Z")
  }
]);

database.courses.insertMany([
  {
    _id: ids.course1,
    slug: "mongodb-for-lms",
    title: "MongoDB для LMS",
    description: "Документная модель, индексы и запросы на примере учебной платформы.",
    authorId: ids.instructor1,
    status: "PUBLISHED",
    tags: ["mongodb", "database", "backend"],
    settings: { language: "ru", difficulty: "beginner", certificate: true },
    createdAt: new Date("2026-03-01T09:00:00Z")
  },
  {
    _id: ids.course2,
    slug: "rest-api-practice",
    title: "REST API на практике",
    description: "Проектирование и проверка HTTP API без лишней магии.",
    authorId: ids.instructor1,
    status: "PUBLISHED",
    tags: ["api", "http"],
    settings: { language: "ru", difficulty: "beginner", certificate: true },
    createdAt: new Date("2026-03-02T09:00:00Z")
  },
  {
    _id: ids.course3,
    slug: "product-analytics",
    title: "Продуктовая аналитика",
    description: "Метрики, события и аккуратное чтение пользовательского поведения.",
    authorId: ids.instructor2,
    status: "PUBLISHED",
    tags: ["analytics", "product"],
    settings: { language: "ru", difficulty: "middle", certificate: false },
    createdAt: new Date("2026-03-03T09:00:00Z")
  },
  {
    _id: ids.course4,
    slug: "ux-basics",
    title: "Основы UX",
    description: "Как делать интерфейсы, которыми не больно пользоваться.",
    authorId: ids.instructor2,
    status: "PUBLISHED",
    tags: ["ux", "design"],
    settings: { language: "ru", difficulty: "beginner", certificate: true },
    createdAt: new Date("2026-03-04T09:00:00Z")
  },
  {
    _id: ids.course5,
    slug: "sql-to-nosql",
    title: "От SQL к NoSQL",
    description: "Когда нормализация помогает, а когда начинает мешать.",
    authorId: ids.instructor1,
    status: "DRAFT",
    tags: ["sql", "nosql"],
    settings: { language: "ru", difficulty: "middle", certificate: false },
    createdAt: new Date("2026-03-05T09:00:00Z")
  },
  {
    _id: ids.course6,
    slug: "docker-for-students",
    title: "Docker для учебных проектов",
    description: "Контейнеры, compose-файлы и воспроизводимые окружения.",
    authorId: ids.instructor1,
    status: "PUBLISHED",
    tags: ["docker", "devops"],
    settings: { language: "ru", difficulty: "beginner", certificate: true },
    createdAt: new Date("2026-03-06T09:00:00Z")
  },
  {
    _id: ids.course7,
    slug: "testing-api",
    title: "Тестирование API",
    description: "Smoke-тесты, негативные сценарии и фикстуры.",
    authorId: ids.instructor1,
    status: "PUBLISHED",
    tags: ["testing", "api"],
    settings: { language: "ru", difficulty: "middle", certificate: true },
    createdAt: new Date("2026-03-07T09:00:00Z")
  },
  {
    _id: ids.course8,
    slug: "clean-frontend",
    title: "Чистый frontend",
    description: "Компоненты, состояния и здравый смысл в интерфейсах.",
    authorId: ids.instructor2,
    status: "PUBLISHED",
    tags: ["frontend", "ui"],
    settings: { language: "ru", difficulty: "middle", certificate: true },
    createdAt: new Date("2026-03-08T09:00:00Z")
  },
  {
    _id: ids.course9,
    slug: "architecture-notes",
    title: "Заметки по архитектуре",
    description: "Границы сервисов, данные и простые решения до сложных.",
    authorId: ids.instructor1,
    status: "ARCHIVED",
    tags: ["architecture"],
    settings: { language: "ru", difficulty: "advanced", certificate: false },
    createdAt: new Date("2026-03-09T09:00:00Z")
  },
  {
    _id: ids.course10,
    slug: "course-design",
    title: "Проектирование учебного курса",
    description: "Как разложить знания на уроки, задания и понятную траекторию.",
    authorId: ids.instructor2,
    status: "PUBLISHED",
    tags: ["education", "methodology"],
    settings: { language: "ru", difficulty: "beginner", certificate: false },
    createdAt: new Date("2026-03-10T09:00:00Z")
  }
]);

database.lessons.insertMany([
  {
    _id: ObjectId("662000000000000000000001"),
    courseId: ids.course1,
    title: "Зачем LMS документная модель",
    description: "Сравниваем таблицы и документы на живом домене.",
    position: 1,
    durationMinutes: 35,
    content: { blocks: [{ type: "text", value: "Вводная часть" }, { type: "quiz", questions: 4 }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-11T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000002"),
    courseId: ids.course1,
    title: "Индексы и частые запросы",
    description: "Подбираем индексы под API, а не просто на всякий случай.",
    position: 2,
    durationMinutes: 45,
    content: { blocks: [{ type: "video", minutes: 18 }, { type: "text", value: "Конспект" }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-12T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000003"),
    courseId: ids.course2,
    title: "HTTP методы",
    description: "GET, POST, PATCH и DELETE без путаницы.",
    position: 1,
    durationMinutes: 30,
    content: { blocks: [{ type: "text", value: "Методы запроса" }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-13T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000004"),
    courseId: ids.course2,
    title: "Коды ответов",
    description: "Когда возвращать 201, 400, 404 и 409.",
    position: 2,
    durationMinutes: 28,
    content: { blocks: [{ type: "quiz", questions: 6 }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-14T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000005"),
    courseId: ids.course3,
    title: "События продукта",
    description: "Что логировать и почему это не равно слежке за всем подряд.",
    position: 1,
    durationMinutes: 40,
    content: { blocks: [{ type: "text", value: "Event naming" }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-15T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000006"),
    courseId: ids.course4,
    title: "Пользовательские сценарии",
    description: "Собираем flow и ищем лишние шаги.",
    position: 1,
    durationMinutes: 32,
    content: { blocks: [{ type: "video", minutes: 14 }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-16T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000007"),
    courseId: ids.course6,
    title: "Первый docker-compose",
    description: "Запускаем API и базу данных одной командой.",
    position: 1,
    durationMinutes: 38,
    content: { blocks: [{ type: "text", value: "Compose basics" }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-17T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000008"),
    courseId: ids.course7,
    title: "Smoke-тесты",
    description: "Минимальная проверка, которая быстро ловит поломки.",
    position: 1,
    durationMinutes: 25,
    content: { blocks: [{ type: "text", value: "Happy path" }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-18T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000009"),
    courseId: ids.course8,
    title: "Состояния компонента",
    description: "Loading, empty, error и обычное состояние.",
    position: 1,
    durationMinutes: 34,
    content: { blocks: [{ type: "text", value: "State design" }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-19T09:00:00Z")
  },
  {
    _id: ObjectId("662000000000000000000010"),
    courseId: ids.course10,
    title: "План курса",
    description: "Сначала цель, потом уроки и задания.",
    position: 1,
    durationMinutes: 29,
    content: { blocks: [{ type: "text", value: "Learning path" }] },
    status: "PUBLISHED",
    createdAt: new Date("2026-03-20T09:00:00Z")
  }
]);

database.enrollments.insertMany([
  {
    _id: ObjectId("663000000000000000000001"),
    userId: ids.student1,
    courseId: ids.course1,
    status: "ACTIVE",
    progress: { completedLessonIds: [ObjectId("662000000000000000000001")], percent: 50 },
    enrolledAt: new Date("2026-03-21T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000002"),
    userId: ids.student1,
    courseId: ids.course2,
    status: "ACTIVE",
    progress: { completedLessonIds: [], percent: 0 },
    enrolledAt: new Date("2026-03-22T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000003"),
    userId: ids.student2,
    courseId: ids.course3,
    status: "COMPLETED",
    progress: { completedLessonIds: [ObjectId("662000000000000000000005")], percent: 100 },
    enrolledAt: new Date("2026-03-23T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000004"),
    userId: ids.student2,
    courseId: ids.course4,
    status: "ACTIVE",
    progress: { completedLessonIds: [], percent: 0 },
    enrolledAt: new Date("2026-03-24T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000005"),
    userId: ids.student3,
    courseId: ids.course6,
    status: "ACTIVE",
    progress: { completedLessonIds: [], percent: 0 },
    enrolledAt: new Date("2026-03-25T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000006"),
    userId: ids.student3,
    courseId: ids.course7,
    status: "ACTIVE",
    progress: { completedLessonIds: [], percent: 0 },
    enrolledAt: new Date("2026-03-26T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000007"),
    userId: ids.student4,
    courseId: ids.course1,
    status: "ACTIVE",
    progress: { completedLessonIds: [], percent: 0 },
    enrolledAt: new Date("2026-03-27T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000008"),
    userId: ids.student5,
    courseId: ids.course5,
    status: "CANCELLED",
    progress: { completedLessonIds: [], percent: 0 },
    enrolledAt: new Date("2026-03-28T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000009"),
    userId: ids.student5,
    courseId: ids.course8,
    status: "ACTIVE",
    progress: { completedLessonIds: [ObjectId("662000000000000000000009")], percent: 100 },
    enrolledAt: new Date("2026-03-29T09:00:00Z")
  },
  {
    _id: ObjectId("663000000000000000000010"),
    userId: ids.student7,
    courseId: ids.course10,
    status: "ACTIVE",
    progress: { completedLessonIds: [], percent: 0 },
    enrolledAt: new Date("2026-03-30T09:00:00Z")
  }
]);

print("Test data inserted: users=10, courses=10, lessons=10, enrollments=10.");
