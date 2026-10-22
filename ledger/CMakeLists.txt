cmake_minimum_required(VERSION 3.10)
project(LedgerSimulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

# Needs the OpenSSL development package (Ubuntu/Debian: sudo apt install libssl-dev).
find_package(OpenSSL REQUIRED)

# Everything except main() lives in a library so tests can link it too.
add_library(ledger_core
    src/Hasher.cpp
    src/Signer.cpp
    src/Wallet.cpp
    src/Transaction.cpp
    src/Block.cpp
    src/Blockchain.cpp
    src/Node.cpp
)
target_include_directories(ledger_core PUBLIC include)
target_link_libraries(ledger_core PUBLIC OpenSSL::Crypto)

add_executable(ledger main.cpp)
target_link_libraries(ledger ledger_core)

enable_testing()
foreach(t hasher signer transaction block difficulty ledger chain)
  add_executable(test_${t} tests/test_${t}.cpp)
  target_link_libraries(test_${t} ledger_core)
  target_compile_options(test_${t} PRIVATE -UNDEBUG)   # tests rely on assert(), keep it enabled
  add_test(NAME ${t} COMMAND test_${t})
endforeach()
