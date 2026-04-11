FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libpoco-dev \
    libpqxx-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build-docker -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build-docker --config Release -j"$(nproc)"

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libpoco-dev \
    libpqxx-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /app/build-docker/lms_api /app/lms_api

ENV PORT=8080
ENV DATABASE_URL=postgresql://lms:lms@postgres:5432/lms_db

EXPOSE 8080

CMD ["./lms_api"]
