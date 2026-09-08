# syntax=docker/dockerfile:1

FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        default-libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN make clean && make DEBUG=0

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        curl \
        libmariadb3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /app/build/server ./server
COPY --from=build /app/root ./root

ENV DB_HOST=db \
    DB_PORT=3306 \
    DB_USER=crypt_user \
    DB_PASSWORD=crypt_password \
    DB_NAME=crypt_server

EXPOSE 9906

CMD ["./server", "-p", "9906"]
