workspace "Homework 01 - Learning Management System" "C4 model for learning management system (variant 19)" {

    model {
        student = person "Student" "Студент, который просматривает курсы, записывается на них и проходит уроки."
        instructor = person "Instructor" "Преподаватель, который создает курсы и добавляет в них уроки."
        administrator = person "Administrator" "Администратор, который управляет пользователями и выполняет поиск пользователей."

        emailProvider = softwareSystem "Email Notification Provider" "Отправляет email-уведомления студентам о записи на курс и завершении обучения." "External System"
        messageBroker = softwareSystem "Message Broker" "Передает асинхронные события между сервисами системы." "External System"

        learningManagementSystem = softwareSystem "Learning Management System" "Система управления обучением." {
            webApp = container "Web Application" "Пользовательский интерфейс для студента, преподавателя и администратора." "React SPA" "Web Application"
            apiGateway = container "API Gateway / BFF" "Единая точка входа, аутентификация и маршрутизация запросов к доменным сервисам." "Node.js REST API" "API"
            userService = container "User Service" "Управление пользователями и поиск пользователей по логину и маске ФИО." "REST API" "Service"
            courseService = container "Course Service" "Создание курсов, добавление уроков и получение каталога курсов." "REST API" "Service"
            learningService = container "Learning Service" "Запись пользователя на курс, список курсов пользователя и отметка о прохождении уроков." "REST API" "Service"
            notificationWorker = container "Notification Worker" "Обрабатывает события обучения и отправляет email-уведомления." "Async Worker" "Worker"
            database = container "Relational Database" "Хранит данные users, courses, lessons, enrollments, lesson_completions с логическим разделением по схемам user/course/learning." "PostgreSQL" "Database"
        }

        student -> learningManagementSystem "Просматривает каталог курсов, записывается и проходит обучение" "HTTPS"
        instructor -> learningManagementSystem "Создает курсы и добавляет уроки" "HTTPS"
        administrator -> learningManagementSystem "Управляет пользователями" "HTTPS"
        learningManagementSystem -> emailProvider "Отправляет email-уведомления пользователям" "HTTPS/REST"
        learningManagementSystem -> messageBroker "Публикует и потребляет доменные события" "AMQP/Kafka protocol"

        student -> webApp "Использует web-интерфейс студента" "HTTPS"
        instructor -> webApp "Использует web-интерфейс преподавателя" "HTTPS"
        administrator -> webApp "Использует web-интерфейс администратора" "HTTPS"
        webApp -> apiGateway "Вызывает backend API" "HTTPS/JSON"

        apiGateway -> userService "Маршрутизирует user-запросы" "HTTPS/REST"
        apiGateway -> courseService "Маршрутизирует course-запросы" "HTTPS/REST"
        apiGateway -> learningService "Маршрутизирует learning-запросы" "HTTPS/REST"

        learningService -> userService "Проверяет существование и статус пользователя" "HTTPS/REST"
        learningService -> courseService "Проверяет существование курса, список уроков и доступность записи" "HTTPS/REST"

        userService -> database "Читает/записывает пользователей (схема user)" "SQL"
        courseService -> database "Читает/записывает курсы и уроки (схема course)" "SQL"
        learningService -> database "Читает/записывает записи на курсы и прогресс (схема learning)" "SQL"

        learningService -> messageBroker "Публикует события UserEnrolled/LessonCompleted (transactional outbox)" "Async event publish"
        notificationWorker -> messageBroker "Потребляет события обучения (idempotent consume)" "Async event consume"
        notificationWorker -> emailProvider "Отправляет email-уведомления" "HTTPS/REST"
    }

    views {
        systemContext learningManagementSystem "SystemContext" "C1: Контекст системы управления обучением." {
            include student
            include instructor
            include administrator
            include learningManagementSystem
            include emailProvider
            include messageBroker
            autolayout lr
        }

        container learningManagementSystem "Container" "C2: Контейнеры LMS и их взаимодействия." {
            include *
            autolayout lr
        }

        dynamic learningManagementSystem "EnrollStudent" "Dynamic: сценарий записи пользователя на курс." {
            student -> webApp "1. Выбирает курс и нажимает кнопку записи" "HTTPS"
            webApp -> apiGateway "2. Отправляет POST /courses/{courseId}/enrollments" "HTTPS/JSON"
            apiGateway -> learningService "3. Передает команду записи на курс" "HTTPS/REST"
            learningService -> userService "4. Проверяет существование и статус студента" "HTTPS/REST"
            learningService -> courseService "5. Проверяет существование курса и статус PUBLISHED" "HTTPS/REST"
            learningService -> database "6. Сохраняет enrollment со статусом ACTIVE" "SQL"
            learningService -> messageBroker "7. Публикует событие UserEnrolled (transactional outbox)" "Async event publish"
            apiGateway -> webApp "8. Возвращает подтверждение записи без ожидания уведомления" "HTTPS/JSON"
            notificationWorker -> messageBroker "9. Потребляет событие UserEnrolled идемпотентно" "Async event consume"
            notificationWorker -> emailProvider "10. Отправляет письмо о записи на курс" "HTTPS/REST"
            autolayout lr
        }

        styles {
            element "Person" {
                background "#0B5D91"
                color "#FFFFFF"
                shape Person
            }
            element "Software System" {
                background "#2E7D32"
                color "#FFFFFF"
            }
            element "External System" {
                background "#616161"
                color "#FFFFFF"
            }
            element "Container" {
                background "#1565C0"
                color "#FFFFFF"
            }
            element "Database" {
                shape Cylinder
                background "#455A64"
                color "#FFFFFF"
            }
            element "Worker" {
                background "#6A1B9A"
                color "#FFFFFF"
            }
            relationship "Relationship" {
                color "#424242"
                thickness 2
            }
        }
    }

    configuration {
        scope softwaresystem
    }
}
