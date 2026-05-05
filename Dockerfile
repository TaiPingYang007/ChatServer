FROM ubuntu:22.04

# 这是为了让 apt-get install 在构建镜像时不要弹交互界面卡住。
ENV DEBIAN_FRONTEND=noninteractive
ARG MUDUO_VERSION=v2.0.2
ARG NLOHMANN_JSON_VERSION=v3.11.3

WORKDIR /app

# Build toolchain + native dependencies required by ChatServer and ChatClient.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    pkg-config \
    libboost-dev \
    libboost-test-dev \
    default-libmysqlclient-dev \
    libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

ENV MUDUO_ROOT=/app/.deps/muduo/install
ENV FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=/app/.deps/nlohmann-json/src

RUN mkdir -p /app/.deps/muduo /app/.deps/nlohmann-json \
    && git clone --branch "${MUDUO_VERSION}" --depth 1 https://github.com/chenshuo/muduo.git /app/.deps/muduo/src \
    && cmake -S /app/.deps/muduo/src -B /app/.deps/muduo/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${MUDUO_ROOT}" \
    && cmake --build /app/.deps/muduo/build -j"$(nproc)" \
    && cmake --install /app/.deps/muduo/build \
    && git clone --branch "${NLOHMANN_JSON_VERSION}" --depth 1 https://github.com/nlohmann/json.git "${FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON}" \
    && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
        -DMUDUO_ROOT="${MUDUO_ROOT}" \
        -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON="${FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON}" \
    && cmake --build build -j"$(nproc)"

EXPOSE 6000 6002

CMD ["./bin/ChatServer", "0.0.0.0", "6000"]
