FROM alpine:3.20

WORKDIR /app

RUN apk add --no-cache g++

COPY src ./src

RUN g++ -std=c++17 -O2 -Wall -Wextra src/http_client.cpp src/rabbitmq_client.cpp src/events.cpp src/producer.cpp -o producer \
    && g++ -std=c++17 -O2 -Wall -Wextra src/http_client.cpp src/rabbitmq_client.cpp src/events.cpp src/consumer.cpp -o consumer

CMD ["./consumer"]
