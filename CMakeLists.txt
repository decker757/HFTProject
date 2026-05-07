cmake_minimum_required(VERSION 3.20)
project(TradingSystem)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- dependencies ----------
if(APPLE)
    set(BOOST_ROOT /opt/homebrew/opt/boost)
    set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@3)
    set(PostgreSQL_ROOT /opt/homebrew/opt/postgresql@18)
endif()

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PostgreSQL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    dotenv
    GIT_REPOSITORY https://github.com/laserpants/dotenv-cpp.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(dotenv)

# ---------- core shared library ----------
add_library(core
    src/core/Logging.cpp
    src/exchange/BinanceWS.cpp
    src/strategy/MACrossStrategy.cpp
    src/strategy/BollingerBandsStrategy.cpp
    src/strategy/MomentumStrategy.cpp
    src/strategy/RSIStrategy.cpp
    src/strategy/StrategyEvaluator.cpp
    src/execution/ExecutionEngine.cpp
    src/market_data/MarketDataStore.cpp
)

target_include_directories(core PUBLIC include/ ${dotenv_SOURCE_DIR}/include)

target_link_libraries(core PUBLIC
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    PostgreSQL::PostgreSQL
)

# ---------- apps ----------
foreach(APP recorder replayer trader)
    add_executable(${APP} apps/${APP}/main.cpp)
    target_link_libraries(${APP} PRIVATE core dotenv)
endforeach()
