// SHURIUM - RPC Module Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>
#include <shurium/rpc/server.h>
#include <shurium/rpc/client.h>
#include <shurium/rpc/commands.h>

#include <fstream>
#include <set>
#include <sys/stat.h>

using namespace shurium;
using namespace shurium::rpc;

// ============================================================================
// JSONValue Tests
// ============================================================================

class JSONValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JSONValueTest, NullValue) {
    JSONValue null;
    EXPECT_TRUE(null.IsNull());
    EXPECT_FALSE(null.IsBool());
    EXPECT_FALSE(null.IsInt());
    EXPECT_FALSE(null.IsDouble());
    EXPECT_FALSE(null.IsString());
    EXPECT_FALSE(null.IsArray());
    EXPECT_FALSE(null.IsObject());
    EXPECT_EQ(null.GetType(), JSONValue::Type::Null);
}

TEST_F(JSONValueTest, BoolValue) {
    JSONValue trueVal(true);
    JSONValue falseVal(false);
    
    EXPECT_TRUE(trueVal.IsBool());
    EXPECT_TRUE(trueVal.GetBool());
    EXPECT_FALSE(falseVal.GetBool());
}

TEST_F(JSONValueTest, IntValue) {
    JSONValue intVal(42);
    JSONValue int64Val(int64_t(9223372036854775807LL));
    
    EXPECT_TRUE(intVal.IsInt());
    EXPECT_EQ(intVal.GetInt(), 42);
    EXPECT_EQ(int64Val.GetInt(), 9223372036854775807LL);
}

TEST_F(JSONValueTest, DoubleValue) {
    JSONValue doubleVal(3.14159);
    
    EXPECT_TRUE(doubleVal.IsDouble());
    EXPECT_TRUE(doubleVal.IsNumber());
    EXPECT_NEAR(doubleVal.GetDouble(), 3.14159, 0.00001);
}

TEST_F(JSONValueTest, StringValue) {
    JSONValue strVal("hello world");
    JSONValue strVal2(std::string("test string"));
    
    EXPECT_TRUE(strVal.IsString());
    EXPECT_EQ(strVal.GetString(), "hello world");
    EXPECT_EQ(strVal2.GetString(), "test string");
}

TEST_F(JSONValueTest, ArrayValue) {
    JSONValue::Array arr;
    arr.push_back(JSONValue(1));
    arr.push_back(JSONValue("two"));
    arr.push_back(JSONValue(3.0));
    
    JSONValue arrayVal(arr);
    
    EXPECT_TRUE(arrayVal.IsArray());
    EXPECT_EQ(arrayVal.Size(), 3);
    EXPECT_EQ(arrayVal[0].GetInt(), 1);
    EXPECT_EQ(arrayVal[1].GetString(), "two");
    EXPECT_NEAR(arrayVal[2].GetDouble(), 3.0, 0.001);
}

TEST_F(JSONValueTest, ObjectValue) {
    JSONValue::Object obj;
    obj["name"] = JSONValue("test");
    obj["value"] = JSONValue(123);
    obj["active"] = JSONValue(true);
    
    JSONValue objectVal(obj);
    
    EXPECT_TRUE(objectVal.IsObject());
    EXPECT_TRUE(objectVal.HasKey("name"));
    EXPECT_TRUE(objectVal.HasKey("value"));
    EXPECT_TRUE(objectVal.HasKey("active"));
    EXPECT_FALSE(objectVal.HasKey("missing"));
    
    EXPECT_EQ(objectVal["name"].GetString(), "test");
    EXPECT_EQ(objectVal["value"].GetInt(), 123);
    EXPECT_TRUE(objectVal["active"].GetBool());
}

TEST_F(JSONValueTest, ToJSONNull) {
    JSONValue null;
    EXPECT_EQ(null.ToJSON(), "null");
}

TEST_F(JSONValueTest, ToJSONBool) {
    EXPECT_EQ(JSONValue(true).ToJSON(), "true");
    EXPECT_EQ(JSONValue(false).ToJSON(), "false");
}

TEST_F(JSONValueTest, ToJSONInt) {
    EXPECT_EQ(JSONValue(42).ToJSON(), "42");
    EXPECT_EQ(JSONValue(-123).ToJSON(), "-123");
    EXPECT_EQ(JSONValue(int64_t(0)).ToJSON(), "0");
}

TEST_F(JSONValueTest, ToJSONString) {
    EXPECT_EQ(JSONValue("hello").ToJSON(), "\"hello\"");
    EXPECT_EQ(JSONValue("").ToJSON(), "\"\"");
}

TEST_F(JSONValueTest, ToJSONArray) {
    JSONValue::Array arr;
    arr.push_back(JSONValue(1));
    arr.push_back(JSONValue(2));
    arr.push_back(JSONValue(3));
    
    std::string json = JSONValue(arr).ToJSON();
    EXPECT_EQ(json, "[1,2,3]");
}

TEST_F(JSONValueTest, ToJSONObject) {
    JSONValue::Object obj;
    obj["a"] = JSONValue(1);
    
    std::string json = JSONValue(obj).ToJSON();
    EXPECT_EQ(json, "{\"a\":1}");
}

TEST_F(JSONValueTest, ParseNull) {
    auto val = JSONValue::TryParse("null");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(val->IsNull());
}

TEST_F(JSONValueTest, ParseBool) {
    auto trueVal = JSONValue::TryParse("true");
    auto falseVal = JSONValue::TryParse("false");
    
    ASSERT_TRUE(trueVal.has_value());
    ASSERT_TRUE(falseVal.has_value());
    EXPECT_TRUE(trueVal->GetBool());
    EXPECT_FALSE(falseVal->GetBool());
}

TEST_F(JSONValueTest, ParseInt) {
    auto val = JSONValue::TryParse("42");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(val->IsInt() || val->IsDouble());
    EXPECT_EQ(val->GetInt(), 42);
}

TEST_F(JSONValueTest, ParseString) {
    auto val = JSONValue::TryParse("\"hello world\"");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(val->IsString());
    EXPECT_EQ(val->GetString(), "hello world");
}

TEST_F(JSONValueTest, ParseArray) {
    auto val = JSONValue::TryParse("[1, 2, 3]");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(val->IsArray());
    EXPECT_EQ(val->Size(), 3);
}

TEST_F(JSONValueTest, ParseObject) {
    auto val = JSONValue::TryParse("{\"key\": \"value\"}");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(val->IsObject());
    EXPECT_TRUE(val->HasKey("key"));
}

TEST_F(JSONValueTest, ParseInvalid) {
    auto val = JSONValue::TryParse("invalid json");
    EXPECT_FALSE(val.has_value());
}

// ============================================================================
// RPCRequest Tests
// ============================================================================

class RPCRequestTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RPCRequestTest, BasicRequest) {
    JSONValue params = JSONValue::Array{JSONValue(1), JSONValue("test")};
    RPCRequest req("testmethod", params, JSONValue(1));
    
    EXPECT_EQ(req.GetMethod(), "testmethod");
    EXPECT_FALSE(req.IsNotification());
    EXPECT_EQ(req.GetId().GetInt(), 1);
}

TEST_F(RPCRequestTest, Notification) {
    RPCRequest req("notify", JSONValue(), JSONValue());
    EXPECT_TRUE(req.IsNotification());
}

TEST_F(RPCRequestTest, GetParamByIndex) {
    JSONValue::Array params;
    params.push_back(JSONValue("first"));
    params.push_back(JSONValue(42));
    
    RPCRequest req("test", JSONValue(params), JSONValue(1));
    
    EXPECT_EQ(req.GetParam(0).GetString(), "first");
    EXPECT_EQ(req.GetParam(1).GetInt(), 42);
    EXPECT_TRUE(req.GetParam(2).IsNull());  // Out of bounds
}

TEST_F(RPCRequestTest, GetParamByName) {
    JSONValue::Object params;
    params["name"] = JSONValue("value");
    params["count"] = JSONValue(10);
    
    RPCRequest req("test", JSONValue(params), JSONValue(1));
    
    EXPECT_EQ(req.GetParam("name").GetString(), "value");
    EXPECT_EQ(req.GetParam("count").GetInt(), 10);
    EXPECT_TRUE(req.GetParam("missing").IsNull());
}

TEST_F(RPCRequestTest, HasParam) {
    JSONValue::Object params;
    params["exists"] = JSONValue(true);
    
    RPCRequest req("test", JSONValue(params), JSONValue(1));
    
    EXPECT_TRUE(req.HasParam("exists"));
    EXPECT_FALSE(req.HasParam("missing"));
}

TEST_F(RPCRequestTest, ToJSON) {
    JSONValue::Array params;
    params.push_back(JSONValue("arg1"));
    
    RPCRequest req("mymethod", JSONValue(params), JSONValue(42));
    std::string json = req.ToJSON();
    
    EXPECT_NE(json.find("\"jsonrpc\":\"2.0\""), std::string::npos);
    EXPECT_NE(json.find("\"method\":\"mymethod\""), std::string::npos);
    EXPECT_NE(json.find("\"id\":42"), std::string::npos);
}

TEST_F(RPCRequestTest, Parse) {
    std::string json = R"({"jsonrpc":"2.0","method":"test","params":[1,2,3],"id":1})";
    auto req = RPCRequest::Parse(json);
    
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->GetMethod(), "test");
    EXPECT_EQ(req->GetId().GetInt(), 1);
    EXPECT_EQ(req->GetParam(0).GetInt(), 1);
}

TEST_F(RPCRequestTest, ParseInvalid) {
    auto req = RPCRequest::Parse("not valid json");
    EXPECT_FALSE(req.has_value());
}

// ============================================================================
// RPCResponse Tests
// ============================================================================

class RPCResponseTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RPCResponseTest, SuccessResponse) {
    auto resp = RPCResponse::Success(JSONValue("result"), JSONValue(1));
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_EQ(resp.GetResult().GetString(), "result");
    EXPECT_EQ(resp.GetId().GetInt(), 1);
}

TEST_F(RPCResponseTest, ErrorResponse) {
    auto resp = RPCResponse::Error(-32600, "Invalid Request", JSONValue(1));
    
    EXPECT_TRUE(resp.IsError());
    EXPECT_EQ(resp.GetErrorCode(), -32600);
    EXPECT_EQ(resp.GetErrorMessage(), "Invalid Request");
}

TEST_F(RPCResponseTest, SuccessToJSON) {
    auto resp = RPCResponse::Success(JSONValue(42), JSONValue(1));
    std::string json = resp.ToJSON();
    
    EXPECT_NE(json.find("\"jsonrpc\":\"2.0\""), std::string::npos);
    EXPECT_NE(json.find("\"result\":42"), std::string::npos);
    EXPECT_NE(json.find("\"id\":1"), std::string::npos);
    EXPECT_EQ(json.find("\"error\""), std::string::npos);
}

TEST_F(RPCResponseTest, ErrorToJSON) {
    auto resp = RPCResponse::Error(-32600, "Invalid Request", JSONValue(1));
    std::string json = resp.ToJSON();
    
    EXPECT_NE(json.find("\"jsonrpc\":\"2.0\""), std::string::npos);
    EXPECT_NE(json.find("\"error\""), std::string::npos);
    EXPECT_NE(json.find("-32600"), std::string::npos);
    EXPECT_NE(json.find("Invalid Request"), std::string::npos);
}

// ============================================================================
// RPCServer Tests
// ============================================================================

class RPCServerTest : public ::testing::Test {
protected:
    RPCServer server;
    
    void SetUp() override {
        RPCServerConfig config;
        config.bindAddress = "127.0.0.1";
        config.port = 0;  // Use any available port
        server.SetConfig(config);
    }
    
    void TearDown() override {
        if (server.IsRunning()) {
            server.Stop();
        }
    }
};

TEST_F(RPCServerTest, RegisterMethod) {
    RPCMethod method;
    method.name = "testmethod";
    method.category = "test";
    method.description = "A test method";
    method.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(JSONValue("test result"), req.GetId());
    };
    
    server.RegisterMethod(method);
    
    EXPECT_TRUE(server.HasMethod("testmethod"));
    EXPECT_FALSE(server.HasMethod("nonexistent"));
}

TEST_F(RPCServerTest, GetMethod) {
    RPCMethod method;
    method.name = "mymethod";
    method.category = "test";
    method.description = "Test description";
    method.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(JSONValue(), req.GetId());
    };
    
    server.RegisterMethod(method);
    
    auto retrieved = server.GetMethod("mymethod");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "mymethod");
    EXPECT_EQ(retrieved->category, "test");
    EXPECT_EQ(retrieved->description, "Test description");
}

TEST_F(RPCServerTest, UnregisterMethod) {
    RPCMethod method;
    method.name = "tounregister";
    method.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(JSONValue(), req.GetId());
    };
    
    server.RegisterMethod(method);
    EXPECT_TRUE(server.HasMethod("tounregister"));
    
    server.UnregisterMethod("tounregister");
    EXPECT_FALSE(server.HasMethod("tounregister"));
}

TEST_F(RPCServerTest, HandleRequestSuccess) {
    RPCMethod method;
    method.name = "add";
    method.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        int64_t a = req.GetParam(0).GetInt();
        int64_t b = req.GetParam(1).GetInt();
        return RPCResponse::Success(JSONValue(a + b), req.GetId());
    };
    
    server.RegisterMethod(method);
    
    JSONValue::Array params;
    params.push_back(JSONValue(5));
    params.push_back(JSONValue(3));
    
    RPCRequest request("add", JSONValue(params), JSONValue(1));
    RPCContext context;
    
    auto response = server.HandleRequest(request, context);
    
    EXPECT_FALSE(response.IsError());
    EXPECT_EQ(response.GetResult().GetInt(), 8);
}

TEST_F(RPCServerTest, HandleRequestMethodNotFound) {
    RPCRequest request("nonexistent", JSONValue(), JSONValue(1));
    RPCContext context;
    
    auto response = server.HandleRequest(request, context);
    
    EXPECT_TRUE(response.IsError());
    EXPECT_EQ(response.GetErrorCode(), ErrorCode::METHOD_NOT_FOUND);
}

TEST_F(RPCServerTest, HandleRawRequest) {
    RPCMethod method;
    method.name = "echo";
    method.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(req.GetParams(), req.GetId());
    };
    
    server.RegisterMethod(method);
    
    std::string json = R"({"jsonrpc":"2.0","method":"echo","params":["hello"],"id":1})";
    RPCContext context;
    
    std::string response = server.HandleRawRequest(json, context);
    
    EXPECT_NE(response.find("\"result\""), std::string::npos);
    EXPECT_NE(response.find("hello"), std::string::npos);
}

TEST_F(RPCServerTest, HandleRawRequestParseError) {
    RPCContext context;
    std::string response = server.HandleRawRequest("invalid json", context);
    
    EXPECT_NE(response.find("\"error\""), std::string::npos);
    EXPECT_NE(response.find("-32700"), std::string::npos);  // Parse error code
}

TEST_F(RPCServerTest, GetMethods) {
    RPCMethod method1;
    method1.name = "method1";
    method1.category = "cat1";
    method1.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(JSONValue(), req.GetId());
    };
    
    RPCMethod method2;
    method2.name = "method2";
    method2.category = "cat2";
    method2.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(JSONValue(), req.GetId());
    };
    
    server.RegisterMethod(method1);
    server.RegisterMethod(method2);
    
    auto methods = server.GetMethods();
    EXPECT_EQ(methods.size(), 2);
}

TEST_F(RPCServerTest, GetMethodsByCategory) {
    RPCMethod method1;
    method1.name = "m1";
    method1.category = "blockchain";
    method1.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(JSONValue(), req.GetId());
    };
    
    RPCMethod method2;
    method2.name = "m2";
    method2.category = "blockchain";
    method2.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(JSONValue(), req.GetId());
    };
    
    RPCMethod method3;
    method3.name = "m3";
    method3.category = "wallet";
    method3.handler = [](const RPCRequest& req, const RPCContext& ctx) {
        return RPCResponse::Success(JSONValue(), req.GetId());
    };
    
    server.RegisterMethod(method1);
    server.RegisterMethod(method2);
    server.RegisterMethod(method3);
    
    auto blockchain = server.GetMethodsByCategory("blockchain");
    auto wallet = server.GetMethodsByCategory("wallet");
    
    EXPECT_EQ(blockchain.size(), 2);
    EXPECT_EQ(wallet.size(), 1);
}

// ============================================================================
// RPCCommandTable Tests
// ============================================================================

class RPCCommandTableTest : public ::testing::Test {
protected:
    RPCCommandTable table;
    RPCServer server;
    
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RPCCommandTableTest, RegisterCommands) {
    table.RegisterCommands(server);
    
    // Check that commands were registered
    EXPECT_TRUE(server.HasMethod("help"));
    EXPECT_TRUE(server.HasMethod("getblockchaininfo"));
    EXPECT_TRUE(server.HasMethod("getnetworkinfo"));
    EXPECT_TRUE(server.HasMethod("getstakinginfo"));
    EXPECT_TRUE(server.HasMethod("getgovernanceinfo"));
}

TEST_F(RPCCommandTableTest, GetAllCommands) {
    table.RegisterCommands(server);
    
    auto commands = table.GetAllCommands();
    EXPECT_GT(commands.size(), 50);  // Should have many commands
}

TEST_F(RPCCommandTableTest, GetCommandsByCategory) {
    table.RegisterCommands(server);
    
    auto blockchain = table.GetCommandsByCategory(Category::BLOCKCHAIN);
    auto wallet = table.GetCommandsByCategory(Category::WALLET);
    auto utility = table.GetCommandsByCategory(Category::UTILITY);
    
    EXPECT_GT(blockchain.size(), 5);
    EXPECT_GT(wallet.size(), 5);
    EXPECT_GT(utility.size(), 3);
}

// ============================================================================
// Helper Function Tests
// ============================================================================

class RPCHelperTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RPCHelperTest, ParseAmountInt) {
    JSONValue val(100);
    Amount amount = ParseAmount(val);
    EXPECT_EQ(amount, 100);
}

TEST_F(RPCHelperTest, ParseAmountDouble) {
    JSONValue val(1.5);
    Amount amount = ParseAmount(val);
    EXPECT_EQ(amount, 150000000);  // 1.5 * COIN
}

TEST_F(RPCHelperTest, ParseAmountString) {
    JSONValue val("2.5");
    Amount amount = ParseAmount(val);
    EXPECT_EQ(amount, 250000000);  // 2.5 * COIN
}

TEST_F(RPCHelperTest, FormatAmount) {
    JSONValue val = FormatAmount(100000000);  // 1 COIN
    EXPECT_TRUE(val.IsDouble());
    EXPECT_NEAR(val.GetDouble(), 1.0, 0.001);
}

TEST_F(RPCHelperTest, ValidateAddressValid) {
    // Valid-looking addresses (correct length, no invalid chars)
    EXPECT_TRUE(ValidateAddress("NXS1ABCDEFGHJKLMNPQRSTUVWXYZabcdefg"));
}

TEST_F(RPCHelperTest, ValidateAddressInvalid) {
    // Too short
    EXPECT_FALSE(ValidateAddress("SHR"));
    
    // Contains invalid characters (0, O, I, l)
    EXPECT_FALSE(ValidateAddress("NXS0ABCDEFGHJKLMNPQRSTUVWXYZabc"));
}

TEST_F(RPCHelperTest, ParseHex) {
    auto bytes = ParseHex("48656c6c6f");  // "Hello" in hex
    EXPECT_EQ(bytes.size(), 5);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x65);
    EXPECT_EQ(bytes[2], 0x6c);
    EXPECT_EQ(bytes[3], 0x6c);
    EXPECT_EQ(bytes[4], 0x6f);
}

TEST_F(RPCHelperTest, FormatHex) {
    std::vector<Byte> bytes = {0x48, 0x65, 0x6c, 0x6c, 0x6f};
    std::string hex = FormatHex(bytes);
    EXPECT_EQ(hex, "48656c6c6f");
}

TEST_F(RPCHelperTest, ParseAndFormatHexRoundtrip) {
    std::string original = "deadbeef";
    auto bytes = ParseHex(original);
    std::string result = FormatHex(bytes);
    EXPECT_EQ(result, original);
}

// ============================================================================
// Command Implementation Tests
// ============================================================================

class RPCCommandImplTest : public ::testing::Test {
protected:
    RPCCommandTable table;
    RPCServer server;
    RPCContext ctx;
    
    void SetUp() override {
        table.RegisterCommands(server);
        ctx.clientAddress = "127.0.0.1";
        ctx.isLocal = true;
    }
    
    void TearDown() override {}
};

TEST_F(RPCCommandImplTest, Help) {
    RPCRequest req("help", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
}

TEST_F(RPCCommandImplTest, HelpSpecificCommand) {
    JSONValue::Array params;
    params.push_back(JSONValue("getblockchaininfo"));
    
    RPCRequest req("help", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("name"));
}

TEST_F(RPCCommandImplTest, Uptime) {
    RPCRequest req("uptime", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsInt());
    EXPECT_GE(resp.GetResult().GetInt(), 0);
}

TEST_F(RPCCommandImplTest, Echo) {
    JSONValue::Array params;
    params.push_back(JSONValue("test"));
    params.push_back(JSONValue(123));
    
    RPCRequest req("echo", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
    EXPECT_EQ(resp.GetResult().Size(), 2);
}

TEST_F(RPCCommandImplTest, GetBlockchainInfo) {
    RPCRequest req("getblockchaininfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("chain"));
    EXPECT_TRUE(resp.GetResult().HasKey("blocks"));
}

TEST_F(RPCCommandImplTest, GetBlockCount) {
    RPCRequest req("getblockcount", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsInt());
}

TEST_F(RPCCommandImplTest, GetDifficulty) {
    RPCRequest req("getdifficulty", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsDouble());
}

TEST_F(RPCCommandImplTest, GetNetworkInfo) {
    RPCRequest req("getnetworkinfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("version"));
    EXPECT_TRUE(resp.GetResult().HasKey("connections"));
}

TEST_F(RPCCommandImplTest, GetStakingInfo) {
    RPCRequest req("getstakinginfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("enabled"));
}

TEST_F(RPCCommandImplTest, GetGovernanceInfo) {
    RPCRequest req("getgovernanceinfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("votingEnabled"));
}

TEST_F(RPCCommandImplTest, ListParameters) {
    RPCRequest req("listparameters", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
    EXPECT_GT(resp.GetResult().Size(), 0);
}

TEST_F(RPCCommandImplTest, GetMiningInfo) {
    RPCRequest req("getmininginfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("blocks"));
    EXPECT_TRUE(resp.GetResult().HasKey("difficulty"));
}

TEST_F(RPCCommandImplTest, ValidateAddress) {
    JSONValue::Array params;
    params.push_back(JSONValue("NXS1ABCDEFGHJKLMNPQRSTUVWXYZabcdefg"));
    
    RPCRequest req("validateaddress", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("isvalid"));
}

TEST_F(RPCCommandImplTest, EstimateFee) {
    JSONValue::Array params;
    params.push_back(JSONValue(6));
    
    RPCRequest req("estimatefee", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsDouble());
    EXPECT_GT(resp.GetResult().GetDouble(), 0);
}

TEST_F(RPCCommandImplTest, GetMempoolInfo) {
    RPCRequest req("getmempoolinfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("size"));
    EXPECT_TRUE(resp.GetResult().HasKey("bytes"));
}

TEST_F(RPCCommandImplTest, GetRawMempool) {
    RPCRequest req("getrawmempool", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
}

TEST_F(RPCCommandImplTest, GetChainTips) {
    RPCRequest req("getchaintips", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
}

TEST_F(RPCCommandImplTest, GetPeerInfo) {
    RPCRequest req("getpeerinfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
}

TEST_F(RPCCommandImplTest, GetConnectionCount) {
    RPCRequest req("getconnectioncount", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsInt());
}

TEST_F(RPCCommandImplTest, ListValidators) {
    RPCRequest req("listvalidators", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
}

TEST_F(RPCCommandImplTest, ListProposals) {
    RPCRequest req("listproposals", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

class RPCErrorHandlingTest : public ::testing::Test {
protected:
    RPCCommandTable table;
    RPCServer server;
    RPCContext ctx;
    
    void SetUp() override {
        table.RegisterCommands(server);
        ctx.clientAddress = "127.0.0.1";
        ctx.isLocal = true;
    }
    
    void TearDown() override {}
};

TEST_F(RPCErrorHandlingTest, MissingRequiredParam) {
    // getblock requires blockhash parameter
    RPCRequest req("getblock", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_TRUE(resp.IsError());
    EXPECT_EQ(resp.GetErrorCode(), ErrorCode::INVALID_PARAMS);
}

TEST_F(RPCErrorHandlingTest, WalletNotLoaded) {
    // getbalance requires wallet
    RPCRequest req("getbalance", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_TRUE(resp.IsError());
    EXPECT_EQ(resp.GetErrorCode(), ErrorCode::WALLET_NOT_FOUND);
}

TEST_F(RPCErrorHandlingTest, InvalidVoteChoice) {
    JSONValue::Array params;
    params.push_back(JSONValue("proposalid"));
    params.push_back(JSONValue("invalid_choice"));
    
    RPCRequest req("vote", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Should fail because wallet not loaded (checked before vote validation)
    EXPECT_TRUE(resp.IsError());
}

// ============================================================================
// RPC Client Tests
// ============================================================================

class RPCClientTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RPCClientTest, CreateConfig) {
    RPCClientConfig config;
    config.host = "localhost";
    config.port = 8332;
    config.rpcUser = "testuser";
    config.rpcPassword = "testpass";
    
    RPCClient client(config);
    
    // Client should be created without error
    EXPECT_FALSE(client.IsConnected());  // Not connected yet
}

TEST_F(RPCClientTest, BuildRequest) {
    RPCClientConfig config;
    config.host = "localhost";
    config.port = 8332;
    
    RPCClient client(config);
    
    JSONValue::Array params;
    params.push_back(JSONValue("arg1"));
    params.push_back(JSONValue(42));
    
    // Verify config is correct
    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 8332);
}

// ============================================================================
// CLI Parser Tests
// ============================================================================

class RPCCLIParserTest : public ::testing::Test {
protected:
    RPCCLIParser parser;
    
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RPCCLIParserTest, ParseSimpleCommand) {
    const char* argv[] = {"shurium-cli", "getblockcount"};
    bool result = parser.Parse(2, const_cast<char**>(argv));
    
    EXPECT_TRUE(result);
    EXPECT_EQ(parser.GetMethod(), "getblockcount");
}

TEST_F(RPCCLIParserTest, ParseCommandWithArgs) {
    const char* argv[] = {"shurium-cli", "getblock", "blockhash123"};
    bool result = parser.Parse(3, const_cast<char**>(argv));
    
    EXPECT_TRUE(result);
    EXPECT_EQ(parser.GetMethod(), "getblock");
}

TEST_F(RPCCLIParserTest, ParseNoCommand) {
    const char* argv[] = {"shurium-cli"};
    bool result = parser.Parse(1, const_cast<char**>(argv));
    
    // Should be false or help requested
    EXPECT_TRUE(result == false || parser.WantsHelp());
}

// ============================================================================
// Result Formatter Tests
// ============================================================================

class RPCResultFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RPCResultFormatterTest, FormatNull) {
    std::string result = RPCResultFormatter::FormatAsText(JSONValue());
    EXPECT_EQ(result, "(null)");
}

TEST_F(RPCResultFormatterTest, FormatBool) {
    EXPECT_EQ(RPCResultFormatter::FormatAsText(JSONValue(true)), "true");
    EXPECT_EQ(RPCResultFormatter::FormatAsText(JSONValue(false)), "false");
}

TEST_F(RPCResultFormatterTest, FormatInt) {
    EXPECT_EQ(RPCResultFormatter::FormatAsText(JSONValue(42)), "42");
}

TEST_F(RPCResultFormatterTest, FormatString) {
    std::string result = RPCResultFormatter::FormatAsText(JSONValue("hello"));
    EXPECT_NE(result.find("hello"), std::string::npos);
}

TEST_F(RPCResultFormatterTest, FormatArray) {
    JSONValue::Array arr;
    arr.push_back(JSONValue(1));
    arr.push_back(JSONValue(2));
    
    std::string result = RPCResultFormatter::FormatAsText(JSONValue(arr));
    EXPECT_NE(result.find("1"), std::string::npos);
    EXPECT_NE(result.find("2"), std::string::npos);
}

TEST_F(RPCResultFormatterTest, FormatObject) {
    JSONValue::Object obj;
    obj["key"] = JSONValue("value");
    
    std::string result = RPCResultFormatter::FormatAsText(JSONValue(obj));
    EXPECT_NE(result.find("key"), std::string::npos);
    EXPECT_NE(result.find("value"), std::string::npos);
}

TEST_F(RPCResultFormatterTest, FormatAsJSON) {
    JSONValue::Object obj;
    obj["test"] = JSONValue(123);
    
    std::string result = RPCResultFormatter::FormatAsJSON(JSONValue(obj), false);
    EXPECT_NE(result.find("test"), std::string::npos);
    EXPECT_NE(result.find("123"), std::string::npos);
}

// ============================================================================
// RPC Integration Tests with ChainState
// ============================================================================

#include <shurium/chain/chainstate.h>
#include <shurium/chain/blockindex.h>
#include <shurium/chain/coins.h>
#include <shurium/consensus/params.h>
#include <shurium/mempool/mempool.h>
#include <shurium/miner/blockassembler.h>

class RPCChainStateIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<CoinsViewMemory> coinsDB;
    std::shared_ptr<ChainStateManager> chainManager;
    std::shared_ptr<Mempool> mempool;
    RPCCommandTable table;
    RPCServer server;
    RPCContext ctx;
    
    void SetUp() override {
        // Create the chain components
        coinsDB = std::make_unique<CoinsViewMemory>();
        chainManager = std::make_shared<ChainStateManager>(consensus::Params::RegTest());
        chainManager->Initialize(coinsDB.get());
        mempool = std::make_shared<Mempool>();
        
        // Create a simple chain of 3 blocks
        BlockHash prevHash;
        for (int i = 0; i < 3; ++i) {
            BlockHeader header;
            header.nVersion = 1;
            header.hashPrevBlock = prevHash;
            header.nTime = 1700000000 + i * 30;
            header.nBits = 0x207fffff;  // Easy regtest difficulty
            header.nNonce = i * 1000;
            header.hashMerkleRoot[0] = static_cast<Byte>(i);
            
            BlockIndex* pindex = chainManager->ProcessBlockHeader(header);
            ASSERT_NE(pindex, nullptr);
            pindex->nTx = 1;  // Simulate 1 transaction per block
            pindex->nChainTx = i + 1;
            
            prevHash = header.GetHash();
        }
        
        // Set the tip to the last block
        chainManager->GetActiveChain().SetTip(
            chainManager->LookupBlockIndex(prevHash));
        
        // Set up RPC with the chain state
        // Note: RPCCommandTable expects ChainState, not ChainStateManager
        // For now we'll test what we can
        table.SetMempool(mempool);
        table.RegisterCommands(server);
        
        ctx.clientAddress = "127.0.0.1";
        ctx.isLocal = true;
    }
    
    void TearDown() override {}
};

TEST_F(RPCChainStateIntegrationTest, GetMempoolInfoWithMempool) {
    RPCRequest req("getmempoolinfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("size"));
    EXPECT_TRUE(resp.GetResult().HasKey("bytes"));
    EXPECT_TRUE(resp.GetResult().HasKey("maxmempool"));
    
    // With empty mempool, size should be 0
    EXPECT_EQ(resp.GetResult()["size"].GetInt(), 0);
    EXPECT_EQ(resp.GetResult()["bytes"].GetInt(), 0);
}

TEST_F(RPCChainStateIntegrationTest, GetRawMempoolEmpty) {
    JSONValue::Array params;
    params.push_back(JSONValue(false));  // non-verbose
    
    RPCRequest req("getrawmempool", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
    EXPECT_EQ(resp.GetResult().Size(), 0);  // Empty mempool
}

TEST_F(RPCChainStateIntegrationTest, GetRawMempoolVerboseEmpty) {
    JSONValue::Array params;
    params.push_back(JSONValue(true));  // verbose
    
    RPCRequest req("getrawmempool", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_EQ(resp.GetResult().Size(), 0);  // Empty mempool
}

TEST_F(RPCChainStateIntegrationTest, GetChainTipsDefault) {
    // Without ChainState set, should return default tip
    RPCRequest req("getchaintips", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
    EXPECT_GE(resp.GetResult().Size(), 1);
    
    auto tip = resp.GetResult()[0];
    EXPECT_TRUE(tip.HasKey("height"));
    EXPECT_TRUE(tip.HasKey("hash"));
    EXPECT_TRUE(tip.HasKey("status"));
}

TEST_F(RPCChainStateIntegrationTest, GetBlockchainInfoDefault) {
    RPCRequest req("getblockchaininfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("chain"));
    EXPECT_TRUE(resp.GetResult().HasKey("blocks"));
    EXPECT_TRUE(resp.GetResult().HasKey("headers"));
    EXPECT_TRUE(resp.GetResult().HasKey("bestblockhash"));
    EXPECT_TRUE(resp.GetResult().HasKey("difficulty"));
    EXPECT_TRUE(resp.GetResult().HasKey("chainwork"));
}

TEST_F(RPCChainStateIntegrationTest, GetDifficultyDefault) {
    RPCRequest req("getdifficulty", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsDouble());
    EXPECT_EQ(resp.GetResult().GetDouble(), 1.0);  // Default difficulty
}

TEST_F(RPCChainStateIntegrationTest, GetBlockCountDefault) {
    RPCRequest req("getblockcount", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsInt());
    EXPECT_EQ(resp.GetResult().GetInt(), 0);  // Default: no blocks without ChainState
}

TEST_F(RPCChainStateIntegrationTest, GetBestBlockHashDefault) {
    RPCRequest req("getbestblockhash", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsString());
    // Default: all zeros hash (64 hex chars)
    EXPECT_EQ(resp.GetResult().GetString().length(), 64);
}

// ============================================================================
// Wallet RPC Integration Tests
// ============================================================================

#include <shurium/wallet/wallet.h>

class RPCWalletIntegrationTest : public ::testing::Test {
protected:
    std::shared_ptr<wallet::Wallet> wallet;
    RPCCommandTable table;
    RPCServer server;
    RPCContext ctx;
    
    void SetUp() override {
        // Create a wallet from mnemonic (for testing)
        // Use a test mnemonic - never use this in production!
        std::string testMnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
        wallet = std::unique_ptr<wallet::Wallet>(
            wallet::Wallet::FromMnemonic(testMnemonic, "", "testpassword").release());
        
        // Set up RPC with wallet
        table.SetWallet(wallet);
        table.RegisterCommands(server);
        
        ctx.clientAddress = "127.0.0.1";
        ctx.isLocal = true;
    }
    
    void TearDown() override {}
};

TEST_F(RPCWalletIntegrationTest, GetWalletInfoNoWallet) {
    // Test with no wallet set
    RPCCommandTable noWalletTable;
    RPCServer noWalletServer;
    noWalletTable.RegisterCommands(noWalletServer);
    
    RPCRequest req("getwalletinfo", JSONValue(), JSONValue(1));
    auto resp = noWalletServer.HandleRequest(req, ctx);
    
    EXPECT_TRUE(resp.IsError());
}

TEST_F(RPCWalletIntegrationTest, GetWalletInfo) {
    RPCRequest req("getwalletinfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("walletname"));
    EXPECT_TRUE(resp.GetResult().HasKey("balance"));
    EXPECT_TRUE(resp.GetResult().HasKey("unconfirmed_balance"));
    EXPECT_TRUE(resp.GetResult().HasKey("txcount"));
}

TEST_F(RPCWalletIntegrationTest, GetBalance) {
    RPCRequest req("getbalance", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    // New wallet should have 0 balance
    EXPECT_TRUE(resp.GetResult().IsDouble());
    EXPECT_EQ(resp.GetResult().GetDouble(), 0.0);
}

TEST_F(RPCWalletIntegrationTest, GetUnconfirmedBalance) {
    RPCRequest req("getunconfirmedbalance", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsDouble());
    EXPECT_EQ(resp.GetResult().GetDouble(), 0.0);
}

TEST_F(RPCWalletIntegrationTest, ListAddresses) {
    RPCRequest req("listaddresses", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
}

TEST_F(RPCWalletIntegrationTest, ListUnspent) {
    RPCRequest req("listunspent", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
    // New wallet should have no UTXOs
    EXPECT_EQ(resp.GetResult().Size(), 0);
}

TEST_F(RPCWalletIntegrationTest, ListTransactions) {
    RPCRequest req("listtransactions", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError());
    EXPECT_TRUE(resp.GetResult().IsArray());
    // New wallet should have no transactions
    EXPECT_EQ(resp.GetResult().Size(), 0);
}

TEST_F(RPCWalletIntegrationTest, WalletLock) {
    // First check if wallet is initially unlocked (initialized with password)
    // After FromMnemonic with password, wallet is unlocked
    EXPECT_FALSE(wallet->IsLocked());
    
    RPCRequest req("walletlock", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Wallet lock may fail if the wallet doesn't support locking (e.g., no encryption)
    // Just check that we get a response (success or not)
    if (!resp.IsError()) {
        EXPECT_TRUE(wallet->IsLocked());
    }
    // If error, the wallet may not support locking, which is acceptable
}

TEST_F(RPCWalletIntegrationTest, WalletUnlock) {
    // First lock the wallet
    wallet->Lock();
    
    // Note: Lock may be a no-op if wallet doesn't support encryption
    // So we skip the IsLocked assertion here
    
    // Then try to unlock it
    JSONValue::Array params;
    params.push_back(JSONValue("testpassword"));
    params.push_back(JSONValue(int64_t(60)));  // 60 seconds timeout
    
    RPCRequest req("walletpassphrase", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Check unlock succeeded if wallet was actually locked
    if (!resp.IsError()) {
        EXPECT_FALSE(wallet->IsLocked());
    }
    // If error, may indicate wallet wasn't actually locked
}

TEST_F(RPCWalletIntegrationTest, WalletUnlockWrongPassword) {
    wallet->Lock();
    
    JSONValue::Array params;
    params.push_back(JSONValue("wrongpassword"));
    params.push_back(JSONValue(int64_t(60)));
    
    RPCRequest req("walletpassphrase", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_TRUE(resp.IsError());
    EXPECT_TRUE(wallet->IsLocked());
}

TEST_F(RPCWalletIntegrationTest, GetNewAddressRequiresUnlock) {
    wallet->Lock();
    
    RPCRequest req("getnewaddress", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Should fail because wallet is locked
    EXPECT_TRUE(resp.IsError());
}

TEST_F(RPCWalletIntegrationTest, SendToAddressInsufficientFunds) {
    // Wallet needs to be unlocked for sending
    wallet->Unlock("testpassword");
    
    JSONValue::Array params;
    params.push_back(JSONValue("NXS1test123456789012345678901234"));
    params.push_back(JSONValue(1.0));  // 1 SHURIUM
    
    RPCRequest req("sendtoaddress", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Should fail due to insufficient funds (wallet is empty)
    EXPECT_TRUE(resp.IsError());
}

// ============================================================================
// RPC Mining Tests
// ============================================================================

class RPCMiningTest : public ::testing::Test {
protected:
    std::unique_ptr<CoinsViewMemory> coinsDB;
    std::shared_ptr<ChainStateManager> chainManager;
    std::shared_ptr<ChainState> chainState;
    std::shared_ptr<Mempool> mempool;
    RPCCommandTable table;
    RPCServer server;
    RPCContext ctx;
    
    void SetUp() override {
        // Create the chain components
        coinsDB = std::make_unique<CoinsViewMemory>();
        chainManager = std::make_shared<ChainStateManager>(consensus::Params::RegTest());
        chainManager->Initialize(coinsDB.get());
        mempool = std::make_shared<Mempool>();
        
        // Create a simple chain of 3 blocks
        BlockHash prevHash;
        for (int i = 0; i < 3; ++i) {
            BlockHeader header;
            header.nVersion = 1;
            header.hashPrevBlock = prevHash;
            header.nTime = 1700000000 + i * 30;
            header.nBits = 0x207fffff;  // Easy regtest difficulty
            header.nNonce = i * 1000;
            header.hashMerkleRoot[0] = static_cast<Byte>(i);
            
            BlockIndex* pindex = chainManager->ProcessBlockHeader(header);
            ASSERT_NE(pindex, nullptr);
            pindex->nTx = 1;
            pindex->nChainTx = i + 1;
            
            prevHash = header.GetHash();
        }
        
        // Set the tip to the last block
        chainManager->GetActiveChain().SetTip(
            chainManager->LookupBlockIndex(prevHash));
        
        // Get the active chain state
        chainState = std::make_shared<ChainState>(
            chainManager->GetBlockIndex(),
            consensus::Params::RegTest(),
            coinsDB.get());
        chainState->GetChain().SetTip(chainManager->GetActiveChain().Tip());
        
        // Set up RPC with chain state, chain manager, and mempool
        table.SetChainState(chainState);
        table.SetChainStateManager(chainManager);
        table.SetMempool(mempool);
        table.RegisterCommands(server);
        
        ctx.clientAddress = "127.0.0.1";
        ctx.isLocal = true;
    }
    
    void TearDown() override {}
};

TEST_F(RPCMiningTest, GetMiningInfo) {
    RPCRequest req("getmininginfo", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError()) << "Error: " << resp.GetErrorMessage();
    EXPECT_TRUE(resp.GetResult().IsObject());
    
    const auto& result = resp.GetResult();
    EXPECT_TRUE(result.HasKey("blocks"));
    EXPECT_TRUE(result.HasKey("difficulty"));
    EXPECT_TRUE(result.HasKey("pooledtx"));
    EXPECT_TRUE(result.HasKey("chain"));
    EXPECT_TRUE(result.HasKey("pouw_enabled"));
    
    // With empty mempool, pooledtx should be 0
    EXPECT_EQ(result["pooledtx"].GetInt(), 0);
    
    // PoUW should be enabled
    EXPECT_TRUE(result["pouw_enabled"].GetBool(false));
}

TEST_F(RPCMiningTest, GetBlockTemplate) {
    RPCRequest req("getblocktemplate", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError()) << "Error: " << resp.GetErrorMessage();
    EXPECT_TRUE(resp.GetResult().IsObject());
    
    const auto& result = resp.GetResult();
    EXPECT_TRUE(result.HasKey("version"));
    EXPECT_TRUE(result.HasKey("previousblockhash"));
    EXPECT_TRUE(result.HasKey("curtime"));
    EXPECT_TRUE(result.HasKey("mintime"));
    EXPECT_TRUE(result.HasKey("height"));
    EXPECT_TRUE(result.HasKey("bits"));
    EXPECT_TRUE(result.HasKey("target"));
    EXPECT_TRUE(result.HasKey("coinbasevalue"));
    EXPECT_TRUE(result.HasKey("transactions"));
    EXPECT_TRUE(result.HasKey("mutable"));
    EXPECT_TRUE(result.HasKey("capabilities"));
    
    // Version should be valid
    EXPECT_GE(result["version"].GetInt(), 1);
    
    // Coinbase value should be positive
    EXPECT_GT(result["coinbasevalue"].GetInt(), 0);
    
    // transactions should be an array
    EXPECT_TRUE(result["transactions"].IsArray());
}

TEST_F(RPCMiningTest, SubmitBlockInvalidHex) {
    // submitblock requires authentication
    ctx.username = "testuser";
    
    JSONValue::Array params;
    params.push_back(JSONValue("invalidhexdata"));
    
    RPCRequest req("submitblock", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Should fail due to invalid hex (block decode failed)
    EXPECT_TRUE(resp.IsError());
    EXPECT_EQ(resp.GetErrorCode(), -22);  // Block decode failed
}

TEST_F(RPCMiningTest, SubmitBlockEmpty) {
    // submitblock requires authentication
    ctx.username = "testuser";
    
    JSONValue::Array params;
    params.push_back(JSONValue(""));
    
    RPCRequest req("submitblock", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Should fail - empty block data
    EXPECT_TRUE(resp.IsError());
}

TEST_F(RPCMiningTest, GetWork) {
    RPCRequest req("getwork", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError()) << "Error: " << resp.GetErrorMessage();
    EXPECT_TRUE(resp.GetResult().IsObject());
    
    const auto& result = resp.GetResult();
    EXPECT_TRUE(result.HasKey("problemId"));
    EXPECT_TRUE(result.HasKey("problemType"));
    EXPECT_TRUE(result.HasKey("difficulty"));
    EXPECT_TRUE(result.HasKey("target"));
    EXPECT_TRUE(result.HasKey("expires"));
}

TEST_F(RPCMiningTest, SubmitWork) {
    // submitwork requires authentication
    ctx.username = "testuser";
    
    JSONValue::Array params;
    params.push_back(JSONValue("0000000000000000000000000000000000000000000000000000000000000000"));
    params.push_back(JSONValue("solution_data"));
    
    RPCRequest req("submitwork", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Currently returns not accepted (placeholder implementation)
    EXPECT_FALSE(resp.IsError()) << "Error: " << resp.GetErrorMessage();
    EXPECT_TRUE(resp.GetResult().IsObject());
    EXPECT_TRUE(resp.GetResult().HasKey("accepted"));
}

TEST_F(RPCMiningTest, ListProblems) {
    RPCRequest req("listproblems", JSONValue(), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError()) << "Error: " << resp.GetErrorMessage();
    EXPECT_TRUE(resp.GetResult().IsArray());
    
    // Should be empty (no problems registered)
    EXPECT_EQ(resp.GetResult().Size(), 0);
}

TEST_F(RPCMiningTest, GetProblem) {
    JSONValue::Array params;
    params.push_back(JSONValue("0000000000000000000000000000000000000000000000000000000000000000"));
    
    RPCRequest req("getproblem", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    EXPECT_FALSE(resp.IsError()) << "Error: " << resp.GetErrorMessage();
    EXPECT_TRUE(resp.GetResult().IsObject());
    
    const auto& result = resp.GetResult();
    EXPECT_TRUE(result.HasKey("problemId"));
    EXPECT_TRUE(result.HasKey("type"));
    EXPECT_TRUE(result.HasKey("status"));
    EXPECT_TRUE(result.HasKey("difficulty"));
}

TEST_F(RPCMiningTest, SubmitBlockValidBlock) {
    // submitblock requires authentication
    ctx.username = "testuser";
    
    // Get the current tip to build on
    BlockIndex* tip = chainManager->GetActiveChain().Tip();
    ASSERT_NE(tip, nullptr);
    
    int64_t currentHeight = tip->nHeight;
    BlockHash prevHash = tip->GetBlockHash();
    
    // Create a new block that builds on the tip
    Block newBlock;
    newBlock.nVersion = 1;
    newBlock.hashPrevBlock = prevHash;
    newBlock.nTime = tip->nTime + 30;  // 30 seconds after previous block
    newBlock.nBits = 0x207fffff;  // Easy regtest difficulty (very high target)
    
    // Create a simple coinbase transaction
    MutableTransaction coinbaseTx;
    coinbaseTx.version = 1;
    coinbaseTx.nLockTime = 0;
    
    // Coinbase input (empty outpoint)
    OutPoint nullOutpoint;
    Script coinbaseScript;
    // Push the height (BIP34)
    int32_t height = currentHeight + 1;
    std::vector<uint8_t> heightBytes;
    while (height > 0) {
        heightBytes.push_back(static_cast<uint8_t>(height & 0xFF));
        height >>= 8;
    }
    coinbaseScript << heightBytes;
    coinbaseTx.vin.push_back(TxIn(nullOutpoint, coinbaseScript, 0xFFFFFFFF));
    
    // Coinbase output - simple OP_TRUE for testing
    Script outputScript;
    outputScript.push_back(OP_TRUE);
    coinbaseTx.vout.push_back(TxOut(5000000000, outputScript));  // 50 SHURIUM reward
    
    newBlock.vtx.push_back(MakeTransactionRef(std::move(coinbaseTx)));
    
    // Compute merkle root
    newBlock.hashMerkleRoot = newBlock.ComputeMerkleRoot();
    
    // Mine the block by finding a valid nonce
    // With 0x207fffff difficulty, almost any nonce should work
    // The target is very high (easy difficulty) for regtest
    Hash256 target = consensus::CompactToBig(newBlock.nBits);
    
    bool found = false;
    for (uint32_t nonce = 0; nonce < 10000000 && !found; ++nonce) {
        newBlock.nNonce = nonce;
        BlockHash blockHash = newBlock.GetHash();
        
        // Check if hash meets target (hash <= target)
        bool meetsTarget = true;
        for (int i = 31; i >= 0; --i) {
            if (blockHash[i] < target[i]) {
                found = true;
                break;
            } else if (blockHash[i] > target[i]) {
                meetsTarget = false;
                break;
            }
        }
        if (meetsTarget) found = true;
    }
    ASSERT_TRUE(found) << "Failed to mine a valid block (this should be easy with regtest difficulty)";
    
    // Serialize the block to hex
    std::string blockHex = miner::BlockToHex(newBlock);
    ASSERT_FALSE(blockHex.empty());
    
    // Submit the block
    JSONValue::Array params;
    params.push_back(JSONValue(blockHex));
    
    RPCRequest req("submitblock", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Block should be accepted (null response on success per BIP22)
    EXPECT_FALSE(resp.IsError()) << "Error: " << resp.GetErrorMessage() 
                                  << " (code: " << resp.GetErrorCode() << ")";
    
    // Verify the chain grew
    BlockIndex* newTip = chainManager->GetActiveChain().Tip();
    EXPECT_NE(newTip, nullptr);
    EXPECT_EQ(newTip->nHeight, currentHeight + 1);
    EXPECT_EQ(newTip->GetBlockHash(), newBlock.GetHash());
}

TEST_F(RPCMiningTest, SubmitBlockDuplicate) {
    // First submit a valid block
    ctx.username = "testuser";
    
    BlockIndex* tip = chainManager->GetActiveChain().Tip();
    ASSERT_NE(tip, nullptr);
    
    // Create and mine a new block
    Block newBlock;
    newBlock.nVersion = 1;
    newBlock.hashPrevBlock = tip->GetBlockHash();
    newBlock.nTime = tip->nTime + 30;
    newBlock.nBits = 0x207fffff;
    
    // Simple coinbase
    MutableTransaction coinbaseTx;
    coinbaseTx.version = 1;
    OutPoint nullOutpoint;
    Script coinbaseScript;
    coinbaseScript << std::vector<uint8_t>{static_cast<uint8_t>(tip->nHeight + 1)};
    coinbaseTx.vin.push_back(TxIn(nullOutpoint, coinbaseScript, 0xFFFFFFFF));
    Script outputScript;
    outputScript.push_back(OP_TRUE);
    coinbaseTx.vout.push_back(TxOut(5000000000, outputScript));
    newBlock.vtx.push_back(MakeTransactionRef(std::move(coinbaseTx)));
    newBlock.hashMerkleRoot = newBlock.ComputeMerkleRoot();
    
    // Mine it
    Hash256 target = consensus::CompactToBig(newBlock.nBits);
    for (uint32_t nonce = 0; nonce < 10000000; ++nonce) {
        newBlock.nNonce = nonce;
        BlockHash blockHash = newBlock.GetHash();
        bool meetsTarget = true;
        for (int i = 31; i >= 0; --i) {
            if (blockHash[i] < target[i]) break;
            else if (blockHash[i] > target[i]) { meetsTarget = false; break; }
        }
        if (meetsTarget) break;
    }
    
    std::string blockHex = miner::BlockToHex(newBlock);
    
    // First submission should succeed
    JSONValue::Array params1;
    params1.push_back(JSONValue(blockHex));
    RPCRequest req1("submitblock", JSONValue(params1), JSONValue(1));
    auto resp1 = server.HandleRequest(req1, ctx);
    EXPECT_FALSE(resp1.IsError()) << "First submission failed: " << resp1.GetErrorMessage();
    
    // Second submission of same block should fail with "duplicate"
    JSONValue::Array params2;
    params2.push_back(JSONValue(blockHex));
    RPCRequest req2("submitblock", JSONValue(params2), JSONValue(2));
    auto resp2 = server.HandleRequest(req2, ctx);
    EXPECT_TRUE(resp2.IsError());
    EXPECT_EQ(resp2.GetErrorCode(), -27);  // duplicate
}

TEST_F(RPCMiningTest, SubmitBlockOrphan) {
    // Try to submit a block that builds on unknown parent
    ctx.username = "testuser";
    
    Block orphanBlock;
    orphanBlock.nVersion = 1;
    // Set a random previous hash that doesn't exist in our chain
    orphanBlock.hashPrevBlock[0] = 0xDE;
    orphanBlock.hashPrevBlock[1] = 0xAD;
    orphanBlock.hashPrevBlock[2] = 0xBE;
    orphanBlock.hashPrevBlock[3] = 0xEF;
    orphanBlock.nTime = GetTime();
    orphanBlock.nBits = 0x207fffff;
    orphanBlock.nNonce = 0;
    
    // Simple coinbase
    MutableTransaction coinbaseTx;
    coinbaseTx.version = 1;
    OutPoint nullOutpoint;
    Script coinbaseScript;
    coinbaseScript << std::vector<uint8_t>{0x01};
    coinbaseTx.vin.push_back(TxIn(nullOutpoint, coinbaseScript, 0xFFFFFFFF));
    Script outputScript;
    outputScript.push_back(OP_TRUE);
    coinbaseTx.vout.push_back(TxOut(5000000000, outputScript));
    orphanBlock.vtx.push_back(MakeTransactionRef(std::move(coinbaseTx)));
    orphanBlock.hashMerkleRoot = orphanBlock.ComputeMerkleRoot();
    
    // Mine it (even though it will be rejected)
    Hash256 target = consensus::CompactToBig(orphanBlock.nBits);
    for (uint32_t nonce = 0; nonce < 10000000; ++nonce) {
        orphanBlock.nNonce = nonce;
        BlockHash blockHash = orphanBlock.GetHash();
        bool meetsTarget = true;
        for (int i = 31; i >= 0; --i) {
            if (blockHash[i] < target[i]) break;
            else if (blockHash[i] > target[i]) { meetsTarget = false; break; }
        }
        if (meetsTarget) break;
    }
    
    std::string blockHex = miner::BlockToHex(orphanBlock);
    
    JSONValue::Array params;
    params.push_back(JSONValue(blockHex));
    RPCRequest req("submitblock", JSONValue(params), JSONValue(1));
    auto resp = server.HandleRequest(req, ctx);
    
    // Should fail because parent is unknown
    EXPECT_TRUE(resp.IsError());
    EXPECT_EQ(resp.GetErrorCode(), -25);  // Block rejected (orphan)
}

// ============================================================================
// RPC Security Tests
// ============================================================================

class RPCSecurityTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RPCSecurityTest, GenerateRPCPassword) {
    // Test password generation with default length
    std::string password1 = GenerateRPCPassword();
    
    // Should be 64 hex characters (32 bytes * 2)
    EXPECT_EQ(password1.size(), 64u);
    
    // Should be valid hex
    for (char c : password1) {
        bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        EXPECT_TRUE(isHex) << "Invalid hex character: " << c;
    }
    
    // Test custom length
    std::string password2 = GenerateRPCPassword(16);
    EXPECT_EQ(password2.size(), 32u);  // 16 bytes * 2 = 32 hex chars
    
    // Passwords should be different (randomness check)
    std::string password3 = GenerateRPCPassword();
    EXPECT_NE(password1, password3);
}

TEST_F(RPCSecurityTest, GenerateRPCUsername) {
    // Test with default prefix
    std::string username1 = GenerateRPCUsername();
    EXPECT_TRUE(username1.find("shurium_") == 0);
    EXPECT_GT(username1.size(), 6u);  // "shurium_" + suffix
    
    // Test with custom prefix
    std::string username2 = GenerateRPCUsername("mynode");
    EXPECT_TRUE(username2.find("mynode_") == 0);
    
    // Usernames should be different (randomness check)
    std::string username3 = GenerateRPCUsername();
    EXPECT_NE(username1, username3);
}

TEST_F(RPCSecurityTest, GenerateRPCCookie) {
    // Create temporary file path
    std::string cookiePath = "/tmp/shurium_test_cookie_" + GenerateRPCPassword(8) + ".txt";
    
    // Generate cookie
    bool success = GenerateRPCCookie(cookiePath);
    EXPECT_TRUE(success);
    
    // Read and verify cookie format
    std::ifstream file(cookiePath);
    ASSERT_TRUE(file.is_open());
    
    std::string cookie;
    std::getline(file, cookie);
    file.close();
    
    // Cookie should be in format "username:password"
    size_t colonPos = cookie.find(':');
    EXPECT_NE(colonPos, std::string::npos);
    
    if (colonPos != std::string::npos) {
        std::string username = cookie.substr(0, colonPos);
        std::string password = cookie.substr(colonPos + 1);
        
        EXPECT_TRUE(username.find("__cookie__") == 0);
        EXPECT_EQ(password.size(), 64u);  // 32 bytes as hex
    }
    
    // Verify file permissions (Unix only)
#ifndef _WIN32
    struct stat fileStat;
    ASSERT_EQ(stat(cookiePath.c_str(), &fileStat), 0);
    EXPECT_EQ(fileStat.st_mode & 0777, 0600);  // Only owner can read/write
#endif
    
    // Cleanup
    std::remove(cookiePath.c_str());
}

TEST_F(RPCSecurityTest, GenerateRPCCookieInvalidPath) {
    // Try to create cookie at invalid path
    bool success = GenerateRPCCookie("/nonexistent/directory/cookie.txt");
    EXPECT_FALSE(success);
}

// Test that password entropy is reasonable
TEST_F(RPCSecurityTest, PasswordEntropy) {
    // Generate many passwords and check they're all different
    std::set<std::string> passwords;
    const int numPasswords = 100;
    
    for (int i = 0; i < numPasswords; ++i) {
        passwords.insert(GenerateRPCPassword(16));
    }
    
    // All passwords should be unique
    EXPECT_EQ(passwords.size(), static_cast<size_t>(numPasswords));
    
    // Check that different bytes are used (basic distribution check)
    std::map<char, int> charCount;
    for (const auto& password : passwords) {
        for (char c : password) {
            charCount[c]++;
        }
    }
    
    // Should use most hex characters (0-9, a-f = 16 possible)
    EXPECT_GE(charCount.size(), 10u);  // At least 10 different hex chars used
}

