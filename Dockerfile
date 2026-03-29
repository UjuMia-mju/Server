FROM ubuntu:22.04

# C++17, CMake, Boost (IOCP 서버 필수)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    git \
    libboost-all-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# 빌드
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

EXPOSE 25000 25001
CMD ["./build/gameserver", "--port=25000"]
