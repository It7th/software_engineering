cmake_minimum_required(VERSION 3.16)

project(lms_event_driven_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Poco REQUIRED COMPONENTS Foundation JSON Net)
find_library(RABBITMQ_LIBRARY rabbitmq REQUIRED)

add_library(lms_events
    src/events.cpp
    src/rabbitmq_client.cpp
)

target_link_libraries(lms_events
    PUBLIC
        Poco::Foundation
        Poco::JSON
        Poco::Net
        ${RABBITMQ_LIBRARY}
)

add_executable(producer src/producer.cpp)
target_link_libraries(producer PRIVATE lms_events)

add_executable(consumer src/consumer.cpp)
target_link_libraries(consumer PRIVATE lms_events)
