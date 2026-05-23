FROM alpine:3.20

WORKDIR /app

RUN apk add --no-cache g++ poco-dev rabbitmq-c-dev

COPY src ./src

RUN g++ -std=c++17 -O2 -Wall -Wextra src/rabbitmq_client.cpp src/events.cpp src/producer.cpp -lPocoNet -lPocoJSON -lPocoFoundation -lrabbitmq -o producer \
    && g++ -std=c++17 -O2 -Wall -Wextra src/rabbitmq_client.cpp src/events.cpp src/consumer.cpp -lPocoJSON -lPocoFoundation -lrabbitmq -o consumer

CMD ["./consumer"]
