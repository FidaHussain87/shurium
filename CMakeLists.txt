cmake_minimum_required(VERSION 3.16)
project(shurium 
    VERSION 0.1.0 
    LANGUAGES CXX
    DESCRIPTION "SHURIUM - Next Evolution Universal Exchange System"
)

# ============================================================================
# Build Options
# ============================================================================
option(SHURIUM_BUILD_TESTS "Build test suite" ON)
option(SHURIUM_BUILD_DOCS "Build documentation" OFF)
option(SHURIUM_ENABLE_COVERAGE "Enable code coverage" OFF)
option(SHURIUM_SANITIZE "Enable sanitizers" OFF)

# ============================================================================
# C++ Standard and Compiler Settings
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
    )
    
    if(SHURIUM_SANITIZE)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
    
    if(SHURIUM_ENABLE_COVERAGE)
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

# ============================================================================
# Dependencies
# ============================================================================
find_package(Threads REQUIRED)

# OpenSSL (optional, for production crypto)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    add_compile_definitions(SHURIUM_USE_OPENSSL)
else()
    message(STATUS "OpenSSL not found, using built-in crypto")
endif()

# ============================================================================
# Include Directories
# ============================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# ============================================================================
# Module Libraries (each module is a separate static library)
# ============================================================================

# Core module - fundamental types and utilities
add_library(shurium_core STATIC
    src/core/types.cpp
    src/core/serialize.cpp
    src/core/hex.cpp
    src/core/random.cpp
)
target_include_directories(shurium_core PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

# Crypto module - cryptographic primitives
add_library(shurium_crypto STATIC
    src/crypto/sha256.cpp
    src/crypto/ripemd160.cpp
    src/crypto/field.cpp
    src/crypto/poseidon.cpp
    src/crypto/hmac.cpp
    src/crypto/aes.cpp
    src/crypto/secp256k1.cpp
    src/crypto/keys.cpp
)
target_link_libraries(shurium_crypto PUBLIC shurium_core)
if(OpenSSL_FOUND)
    target_link_libraries(shurium_crypto PRIVATE OpenSSL::Crypto)
endif()

# Util module - common utilities
add_library(shurium_util STATIC
    src/util/logging.cpp
    src/util/time.cpp
    src/util/fs.cpp
    src/util/threadpool.cpp
    src/util/config.cpp
)
target_link_libraries(shurium_util PUBLIC shurium_core Threads::Threads)

# Transaction module
add_library(shurium_tx STATIC
    src/core/transaction.cpp
    src/core/script.cpp
)
target_link_libraries(shurium_tx PUBLIC shurium_core shurium_crypto)

# Script interpreter module - script verification engine
add_library(shurium_script STATIC
    src/script/interpreter.cpp
)
target_link_libraries(shurium_script PUBLIC shurium_tx shurium_crypto)

# Block module  
add_library(shurium_block STATIC
    src/core/block.cpp
    src/core/merkle.cpp
)
target_link_libraries(shurium_block PUBLIC shurium_tx)

# Database module - persistent storage
add_library(shurium_db STATIC
    src/db/database.cpp
    src/db/blockdb.cpp
    src/db/utxodb.cpp
)
target_link_libraries(shurium_db PUBLIC shurium_block shurium_util)

# Try to find LevelDB
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LEVELDB QUIET leveldb)
endif()

# Also try manual find
if(NOT LEVELDB_FOUND)
    find_path(LEVELDB_INCLUDE_DIR leveldb/db.h)
    find_library(LEVELDB_LIBRARY leveldb)
    if(LEVELDB_INCLUDE_DIR AND LEVELDB_LIBRARY)
        set(LEVELDB_FOUND TRUE)
        set(LEVELDB_LIBRARIES ${LEVELDB_LIBRARY})
        set(LEVELDB_INCLUDE_DIRS ${LEVELDB_INCLUDE_DIR})
    endif()
endif()

if(LEVELDB_FOUND)
    message(STATUS "LevelDB found")
    target_compile_definitions(shurium_db PRIVATE SHURIUM_USE_LEVELDB)
    target_include_directories(shurium_db PRIVATE ${LEVELDB_INCLUDE_DIRS})
    target_link_libraries(shurium_db PRIVATE ${LEVELDB_LIBRARIES})
else()
    message(STATUS "LevelDB not found, using in-memory database fallback")
endif()

# Chain module - UTXO set, block index, chain state
add_library(shurium_chain STATIC
    src/chain/coins.cpp
    src/chain/blockindex.cpp
    src/chain/chainstate.cpp
)
target_link_libraries(shurium_chain PUBLIC shurium_block shurium_consensus shurium_db shurium_script)

# Mempool module - Transaction memory pool
add_library(shurium_mempool STATIC
    src/mempool/mempool.cpp
)
target_link_libraries(shurium_mempool PUBLIC shurium_chain)

# Miner module - Block assembly and mining
add_library(shurium_miner STATIC
    src/miner/blockassembler.cpp
    src/miner/miner.cpp
)
target_link_libraries(shurium_miner PUBLIC shurium_chain shurium_mempool shurium_consensus shurium_network)

# Identity module - ZK identity system
add_library(shurium_identity STATIC
    src/identity/zkproof.cpp
    src/identity/commitment.cpp
    src/identity/identity.cpp
    src/identity/nullifier.cpp
    src/identity/sigma.cpp
    src/identity/rangeproof.cpp
)
target_link_libraries(shurium_identity PUBLIC shurium_crypto)

# Consensus module - Proof of Useful Work
add_library(shurium_consensus STATIC
    src/consensus/params.cpp
    src/consensus/validation.cpp
    src/consensus/pouw.cpp
    src/consensus/chain.cpp
    src/consensus/checkpoints.cpp
)
target_link_libraries(shurium_consensus PUBLIC shurium_block shurium_identity)

# Marketplace module - Problem marketplace
add_library(shurium_marketplace STATIC
    src/marketplace/problem.cpp
    src/marketplace/solution.cpp
    src/marketplace/verifier.cpp
    src/marketplace/marketplace.cpp
)
target_link_libraries(shurium_marketplace PUBLIC shurium_consensus shurium_crypto shurium_util)

# Economics module - UBI and stability
add_library(shurium_economics STATIC
    src/economics/reward.cpp
    src/economics/ubi.cpp
    src/economics/stability.cpp
    src/economics/oracle.cpp
    src/economics/treasury.cpp
)
target_link_libraries(shurium_economics PUBLIC shurium_consensus shurium_identity)

# Governance module - On-chain governance
add_library(shurium_governance STATIC
    src/governance/governance.cpp
)
target_link_libraries(shurium_governance PUBLIC shurium_crypto shurium_economics)

# Staking module - Proof-of-stake
add_library(shurium_staking STATIC
    src/staking/staking.cpp
)
target_link_libraries(shurium_staking PUBLIC shurium_crypto shurium_economics)

# Network module - P2P networking
add_library(shurium_network STATIC
    src/network/address.cpp
    src/network/message.cpp
    src/network/peer.cpp
    src/network/connection.cpp
    src/network/protocol.cpp
    src/network/sync.cpp
    src/network/message_processor.cpp
    src/network/addrman.cpp
    src/network/network_manager.cpp
    src/network/dnsseed.cpp
)
target_link_libraries(shurium_network PUBLIC shurium_block shurium_util shurium_mempool)

# Wallet module
add_library(shurium_wallet STATIC
    src/wallet/hdkey.cpp
    src/wallet/wallet.cpp
    src/wallet/keystore.cpp
    src/wallet/coinselection.cpp
)
target_link_libraries(shurium_wallet PUBLIC shurium_tx shurium_crypto shurium_identity)
if(OpenSSL_FOUND)
    target_link_libraries(shurium_wallet PRIVATE OpenSSL::Crypto)
endif()

# RPC module
add_library(shurium_rpc STATIC
    src/rpc/server.cpp
    src/rpc/client.cpp
    src/rpc/commands.cpp
)
target_link_libraries(shurium_rpc PUBLIC shurium_util shurium_chain shurium_mempool shurium_wallet shurium_miner)
if(OpenSSL_FOUND)
    target_link_libraries(shurium_rpc PRIVATE OpenSSL::Crypto)
endif()

# Node module - Node context and initialization
add_library(shurium_node STATIC
    src/node/context.cpp
)
target_link_libraries(shurium_node PUBLIC 
    shurium_chain 
    shurium_mempool 
    shurium_network 
    shurium_db
)

# Combined library for convenience
add_library(shurium INTERFACE)
target_link_libraries(shurium INTERFACE
    shurium_core
    shurium_crypto
    shurium_util
    shurium_tx
    shurium_script
    shurium_block
    shurium_db
    shurium_chain
    shurium_mempool
    shurium_identity
    shurium_consensus
    shurium_marketplace
    shurium_economics
    shurium_governance
    shurium_staking
    shurium_network
    shurium_wallet
    shurium_rpc
    shurium_node
)

# ============================================================================
# Executables
# ============================================================================
add_executable(shuriumd src/shuriumd.cpp)
target_link_libraries(shuriumd PRIVATE shurium)

add_executable(shurium-cli src/shurium-cli.cpp)
target_link_libraries(shurium-cli PRIVATE shurium)

add_executable(shurium-wallet-tool src/shurium-wallet.cpp)
target_link_libraries(shurium-wallet-tool PRIVATE shurium)

add_executable(genesis-miner src/genesis-miner.cpp)
target_link_libraries(genesis-miner PRIVATE shurium)

# ============================================================================
# Tests
# ============================================================================
if(SHURIUM_BUILD_TESTS)
    enable_testing()
    
    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    
    include(GoogleTest)
    
    # Test helper library
    add_library(shurium_test_util STATIC
        tests/test_util.cpp
    )
    target_link_libraries(shurium_test_util PUBLIC 
        shurium
        GTest::gtest
        GTest::gtest_main
    )
    
    # Function to add a test
    function(shurium_add_test test_name test_source)
        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} PRIVATE 
            shurium_test_util
            GTest::gtest
            GTest::gtest_main
        )
        gtest_discover_tests(${test_name})
    endfunction()
    
    # Core tests
    shurium_add_test(test_types tests/core/test_types.cpp)
    shurium_add_test(test_serialize tests/core/test_serialize.cpp)
    shurium_add_test(test_hex tests/core/test_hex.cpp)
    shurium_add_test(test_random tests/core/test_random.cpp)
    shurium_add_test(test_script tests/core/test_script.cpp)
    
    # Script interpreter tests
    shurium_add_test(test_interpreter tests/script/test_interpreter.cpp)
    
    # Crypto tests
    shurium_add_test(test_sha256 tests/crypto/test_sha256.cpp)
    shurium_add_test(test_ripemd160 tests/crypto/test_ripemd160.cpp)
    shurium_add_test(test_poseidon tests/crypto/test_poseidon.cpp)
    shurium_add_test(test_keys tests/crypto/test_keys.cpp)
    
    # Transaction tests
    shurium_add_test(test_transaction tests/core/test_transaction.cpp)
    
    # Block tests
    shurium_add_test(test_block tests/core/test_block.cpp)
    shurium_add_test(test_merkle tests/core/test_merkle.cpp)
    
    # Identity tests
    shurium_add_test(test_zkproof tests/identity/test_zkproof.cpp)
    shurium_add_test(test_identity tests/identity/test_identity.cpp)
    
    # Consensus tests
    shurium_add_test(test_consensus tests/consensus/test_consensus.cpp)
    shurium_add_test(test_pouw tests/consensus/test_pouw.cpp)
    shurium_add_test(test_checkpoints tests/consensus/test_checkpoints.cpp)
    
    # Chain tests
    shurium_add_test(test_chain tests/chain/test_chain.cpp)
    
    # Database tests
    shurium_add_test(test_db tests/db/test_db.cpp)
    
    # Mempool tests
    shurium_add_test(test_mempool tests/mempool/test_mempool.cpp)
    
    # Marketplace tests
    shurium_add_test(test_marketplace tests/marketplace/test_marketplace.cpp)
    
    # Network tests
    shurium_add_test(test_network tests/network/test_network.cpp)
    shurium_add_test(test_dnsseed tests/network/test_dnsseed.cpp)
    
    # Wallet tests
    shurium_add_test(test_wallet tests/wallet/test_wallet.cpp)
    
    # Util tests
    shurium_add_test(test_util tests/util/test_util.cpp)
    shurium_add_test(test_config tests/util/test_config.cpp)
    
    # Economics tests
    shurium_add_test(test_reward tests/economics/test_reward.cpp)
    shurium_add_test(test_ubi tests/economics/test_ubi.cpp)
    shurium_add_test(test_stability tests/economics/test_stability.cpp)
    shurium_add_test(test_oracle tests/economics/test_oracle.cpp)
    shurium_add_test(test_treasury tests/economics/test_treasury.cpp)
    
    # Governance tests
    shurium_add_test(test_governance tests/governance/test_governance.cpp)
    
    # Staking tests
    shurium_add_test(test_staking tests/staking/test_staking.cpp)
    
    # RPC tests
    shurium_add_test(test_rpc tests/rpc/test_rpc.cpp)
    
    # Integration tests
    shurium_add_test(test_integration tests/integration/test_integration.cpp)
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS shuriumd shurium-cli shurium-wallet-tool
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/shurium
    DESTINATION include
)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "SHURIUM Configuration Summary")
message(STATUS "===========================")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Tests:          ${SHURIUM_BUILD_TESTS}")
message(STATUS "Coverage:       ${SHURIUM_ENABLE_COVERAGE}")
message(STATUS "Sanitizers:     ${SHURIUM_SANITIZE}")
message(STATUS "OpenSSL:        ${OpenSSL_FOUND}")
message(STATUS "")
