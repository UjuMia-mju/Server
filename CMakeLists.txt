cmake_minimum_required(VERSION 3.16)
project(IOCPGameServer VERSION 1.0.0)

# C++17 필수! (std::thread, std::filesystem, structured binding 등)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Release 모드 최적화 (게임서버 성능 최우선!)
set(CMAKE_BUILD_TYPE Release)

# 소스파일 자동 수집 (main.cpp, *.cpp 모두)
file(GLOB_RECURSE SOURCES 
    "main.cpp"
    "*.cpp"
    "src/*.cpp"
    "iocp/*.cpp"
    "network/*.cpp"
)

# gameserver 실행파일 생성
add_executable(gameserver ${SOURCES})

# IOCP 게임서버 최적화 컴파일 옵션
target_compile_options(gameserver PRIVATE 
    -Wall 
    -Wextra 
    -O3          # 최고 성능 최적화
    -DNDEBUG     # 디버그 제거
    -march=native # CPU 최적화
    -flto        # Link Time Optimization
)

# 멀티스레딩 지원 (pthread 필수!)
target_link_libraries(gameserver 
    pthread
)

# 헤더파일 경로 (include/, . 등)
target_include_directories(gameserver PRIVATE 
    "include"
    "."
    "src"
)

# 게임서버 실행파일에 심볼릭 링크 (편의성)
set_target_properties(gameserver PROPERTIES 
    OUTPUT_NAME "iocp-gameserver"
)

message(STATUS "IOCP GameServer C++17 Build Setting Complete!")
message(STATUS "SourceFile: ${SOURCES}")
