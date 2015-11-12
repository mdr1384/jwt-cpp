#include "gtest/gtest.h"
#include <memory>
#include <string>
#include <array>
#include <vector>
#include "jwt/jwt_all.h"
#include "jwt/setvalidator.h"
#include "constants.h"

class MessageValidatorTest : public ::testing::Test {
public:
  void UnsignedFailsTest(MessageValidator *msg) {
    EXPECT_FALSE(msg->Validate(nullptr, message_, ""));
  }

  void SignSucceeds(MessageSigner *validator) {
    std::string sign = validator->Digest(message_);
    EXPECT_TRUE(validator->Validate(nullptr, message_, sign));
  }

  void TamperResistant(MessageSigner *validator) {
    std::string sign = validator->Digest(message_);
    message_.append("x");
    EXPECT_FALSE(validator->Validate(nullptr, message_, sign));
  }

  void SignOnSubstr(MessageSigner *validator) {
    size_t len = 4096;
    std::unique_ptr<uint8_t[]> pSignature(new uint8_t[len]);
    EXPECT_TRUE(validator->Sign((uint8_t *) message_.c_str(), 6, pSignature.get(), &len));
    EXPECT_TRUE(validator->Verify(nullptr, (uint8_t *) message_.c_str(), 6, pSignature.get(), len));
  }

  void DoubleValidate(MessageSigner *validator) {
    std::string sign = validator->Digest(message_);
    EXPECT_TRUE(validator->Validate(nullptr, message_, sign));
    EXPECT_TRUE(validator->Validate(nullptr, message_, sign));
  }

  std::vector<MessageSigner *> hslist_;
  std::vector<MessageSigner *> rslist_;
  std::string message_;

protected:
  virtual void SetUp() override {
    message_ = "Hello World!";
    hslist_.push_back(new HS256Validator("secret1"));
    hslist_.push_back(new HS384Validator("secret2"));
    hslist_.push_back(new HS512Validator("secret3"));

    rslist_.push_back(new RS256Validator(pubkey, privkey));
    rslist_.push_back(new RS384Validator(pubkey, privkey));
    rslist_.push_back(new RS512Validator(pubkey, privkey));
  }

  virtual void TearDown() override {
    for (auto s : hslist_) {
      delete s;
    }
    hslist_.clear();
    for (auto s : rslist_) {
      delete s;
    }
    rslist_.clear();
  }
};

TEST_F(MessageValidatorTest, hmac_tamper_resistant) {
  for (auto hs : hslist_)
    TamperResistant(hs);
}

TEST_F(MessageValidatorTest, rs_tamper_resistant) {
  for (auto rs : rslist_)
    TamperResistant(rs);
}

TEST_F(MessageValidatorTest, hmac_unsigned_fails) {
  for (auto hs : hslist_)
    UnsignedFailsTest(hs);
}

TEST_F(MessageValidatorTest, rs_unsigned_fails) {
  for (auto rs : rslist_)
    UnsignedFailsTest(rs);
}

TEST_F(MessageValidatorTest, hmac_double_validate) {
  for (auto hs : hslist_)
    DoubleValidate(hs);
}

TEST_F(MessageValidatorTest, rs_double_validate) {
  for (auto rs : rslist_)
    DoubleValidate(rs);
}

TEST_F(MessageValidatorTest, hmac_signing_succeed) {
  for (auto hs : hslist_)
    SignSucceeds(hs);
}

TEST_F(MessageValidatorTest, rs_signing_succeed) {
  for (auto rs : rslist_)
    SignSucceeds(rs);
}

TEST_F(MessageValidatorTest, hmac_signing_on_substr_succeed) {
  for (auto hs : hslist_)
    SignOnSubstr(hs);
}

TEST_F(MessageValidatorTest, rs_signing_on_substr_succeed) {
  for (auto rs : rslist_)
    SignOnSubstr(rs);
}

TEST(nonevalidator_test, signed_fails) {
  NoneValidator validator;
  EXPECT_FALSE(validator.Validate(nullptr, "foo", "bar"));
}

TEST(nonevalidator_test, unsigned_succeeds) {
  NoneValidator validator;
  EXPECT_TRUE(validator.Validate(nullptr, "hello", ""));
}

TEST(kidvalidator_test, same_algs) {
  HS256Validator hs1("secret1");
  HS384Validator hs2("secret2");
  KidValidator kid;
  kid.Register("kid1", &hs1);

  ASSERT_THROW(kid.Register("kid2", &hs2), std::logic_error);
}


TEST(kidvalidator_test, no_kid) {
  KidValidator kid;
  json_ptr json(json_pack("{ss}", "kid", "kid1"));
  EXPECT_FALSE(kid.Validate(json.get(), "", ""));

  json_ptr wrong_type(json_pack("{si}", "kid", 15));
  EXPECT_FALSE(kid.Validate(wrong_type.get(), "", ""));

  json_ptr no_kid(json_pack("{si}", "nokid", 15));
  EXPECT_FALSE(kid.Validate(no_kid.get(), "", ""));
}

TEST(kidvalidator_test, can_register_kid) {
  HS256Validator hs1("secret1");
  HS256Validator hs2("secret2");
  KidValidator kid;
  kid.Register("kid1", &hs1);
  kid.Register("kid2", &hs2);
  std::string message = "Hello World!";
  std::string sig1 = hs1.Digest(message);
  std::string sig2 = hs2.Digest(message);
  json_ptr json(json_pack("{ss}", "kid", "kid1"));

  //Key id set to kid1, so  hs1 should validate
  EXPECT_TRUE(kid.Validate(json.get(), message, sig1));

  //Key id set to kid1, so  hs1 should validate, which
  //should fail
  EXPECT_FALSE(kid.Validate(json.get(), message, sig2));
}

TEST_F(MessageValidatorTest, wrong_algo) {
  std::vector<MessageValidator*> validators(hslist_.begin(), hslist_.end());
  SetValidator set(validators);
  json_ptr json_rs256(json_pack("{ss}", "alg", "RS256"));
  json_ptr json_512(json_pack("{ss}", "foo", "HS512"));

  //Should pick the alg, so  hs1 should validate
  EXPECT_FALSE(set.Validate(json_rs256.get(), "", ""));
  EXPECT_FALSE(set.Validate(json_512.get(), "", ""));
}

TEST_F(MessageValidatorTest, picks_algo) {
  std::vector<MessageValidator*> validators(hslist_.begin(), hslist_.end());
  SetValidator set(validators);
  json_ptr json_256(json_pack("{ss}", "alg", "HS256"));
  json_ptr json_512(json_pack("{ss}", "alg", "HS512"));

  std::string message = "Hello World!";
  std::string sig1 = hslist_.front()->Digest(message);

  //Should pick the alg, so  hs1 should validate
  EXPECT_TRUE(set.Validate(json_256.get(), message, sig1));
  EXPECT_FALSE(set.Validate(json_512.get(), message, sig1));
}

