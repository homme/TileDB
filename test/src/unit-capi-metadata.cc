/**
 * @file unit-capi-metadata.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2017-2019 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * Tests the C API for array metadata.
 */

#include "test/src/helpers.h"
#include "tiledb/sm/c_api/tiledb.h"
#include "tiledb/sm/c_api/tiledb_struct_def.h"

#ifdef _WIN32
#include "tiledb/sm/filesystem/win.h"
#else
#include "tiledb/sm/filesystem/posix.h"
#endif

#include <catch.hpp>
#include <chrono>
#include <iostream>
#include <thread>

using namespace tiledb::sm;

/* ********************************* */
/*         STRUCT DEFINITION         */
/* ********************************* */

struct CMetadataFx {
  tiledb_ctx_t* ctx_;
  tiledb_vfs_t* vfs_;
  bool s3_supported_, hdfs_supported_;
  std::string temp_dir_;
  const std::string s3_bucket_name_ =
      "s3://" + random_bucket_name("tiledb") + "/";
  std::string array_name_;
  const char* ARRAY_NAME = "test_metadata";
  tiledb_array_t* array_ = nullptr;
  const char* key_ = "0123456789abcdeF0123456789abcdeF";
  const uint32_t key_len_ =
      (uint32_t)strlen("0123456789abcdeF0123456789abcdeF");
  const tiledb_encryption_type_t enc_type_ = TILEDB_AES_256_GCM;

  void create_default_array_1d();
  void create_default_array_1d_with_key();

  CMetadataFx();
  ~CMetadataFx();
};

CMetadataFx::CMetadataFx() {
  ctx_ = nullptr;
  vfs_ = nullptr;
  hdfs_supported_ = false;
  s3_supported_ = false;

  get_supported_fs(&s3_supported_, &hdfs_supported_);
  create_ctx_and_vfs(s3_supported_, &ctx_, &vfs_);
  create_s3_bucket(s3_bucket_name_, s3_supported_, ctx_, vfs_);

// Create temporary directory based on the supported filesystem
#ifdef _WIN32
  temp_dir_ = tiledb::sm::Win::current_dir() + "\\tiledb_test\\";
#else
  temp_dir_ = "file://" + tiledb::sm::Posix::current_dir() + "/tiledb_test/";
#endif
  if (s3_supported_)
    temp_dir_ = s3_bucket_name_ + "tiledb/test/";
  if (hdfs_supported_)
    temp_dir_ = "hdfs:///tiledb_test/";
  create_dir(temp_dir_, ctx_, vfs_);

  array_name_ = temp_dir_ + ARRAY_NAME;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array_);
  CHECK(rc == TILEDB_OK);
}

CMetadataFx::~CMetadataFx() {
  tiledb_array_free(&array_);
  remove_dir(temp_dir_, ctx_, vfs_);
  tiledb_ctx_free(&ctx_);
  tiledb_vfs_free(&vfs_);
}

void CMetadataFx::create_default_array_1d() {
  uint64_t domain[] = {1, 10};
  uint64_t tile_extent = 5;
  create_array(
      ctx_,
      array_name_,
      TILEDB_DENSE,
      {"d"},
      {TILEDB_UINT64},
      {domain},
      {&tile_extent},
      {"a", "b", "c"},
      {TILEDB_INT32, TILEDB_CHAR, TILEDB_FLOAT32},
      {1, TILEDB_VAR_NUM, 2},
      {::Compressor(TILEDB_FILTER_NONE, -1),
       ::Compressor(TILEDB_FILTER_ZSTD, -1),
       ::Compressor(TILEDB_FILTER_LZ4, -1)},
      TILEDB_ROW_MAJOR,
      TILEDB_ROW_MAJOR,
      2);
}

void CMetadataFx::create_default_array_1d_with_key() {
  uint64_t domain[] = {1, 10};
  uint64_t tile_extent = 5;
  create_array(
      ctx_,
      array_name_,
      enc_type_,
      key_,
      key_len_,
      TILEDB_DENSE,
      {"d"},
      {TILEDB_UINT64},
      {domain},
      {&tile_extent},
      {"a", "b", "c"},
      {TILEDB_INT32, TILEDB_CHAR, TILEDB_FLOAT32},
      {1, TILEDB_VAR_NUM, 2},
      {::Compressor(TILEDB_FILTER_NONE, -1),
       ::Compressor(TILEDB_FILTER_ZSTD, -1),
       ::Compressor(TILEDB_FILTER_LZ4, -1)},
      TILEDB_ROW_MAJOR,
      TILEDB_ROW_MAJOR,
      2);
}

/* ********************************* */
/*                TESTS              */
/* ********************************* */

TEST_CASE_METHOD(
    CMetadataFx, "C API: Metadata, basic errors", "[capi][metadata][errors]") {
  // Create default array
  create_default_array_1d();

  // Create array
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);

  // Put metadata on an array that is not opened
  int v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "key", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_ERR);

  // Write metadata on an array opened in READ mode
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_put_metadata(ctx_, array, "key", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_ERR);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);

  // Reopen array in WRITE mode
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);

  // Write null key
  rc = tiledb_array_put_metadata(ctx_, array, NULL, TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_ERR);

  // Write null value
  rc = tiledb_array_put_metadata(ctx_, array, "key", TILEDB_INT32, 1, NULL);
  CHECK(rc == TILEDB_ERR);

  // Write zero values
  rc = tiledb_array_put_metadata(ctx_, array, "key", TILEDB_INT32, 0, &v);
  CHECK(rc == TILEDB_ERR);

  // Write value type ANY
  rc = tiledb_array_put_metadata(ctx_, array, "key", TILEDB_ANY, 1, &v);
  CHECK(rc == TILEDB_ERR);

  // Write a correct item
  rc = tiledb_array_put_metadata(ctx_, array, "key", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);

  // Open with key
  rc = tiledb_array_open_with_key(
      ctx_, array, TILEDB_READ, enc_type_, key_, key_len_);
  CHECK(rc == TILEDB_ERR);

  // Clean up
  tiledb_array_free(&array);
}

TEST_CASE_METHOD(
    CMetadataFx, "C API: Metadata, write/read", "[capi][metadata][read]") {
  // Create default array
  create_default_array_1d();

  // Create and open array in write mode
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);

  // Write items
  int32_t v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  float f[] = {1.1f, 1.2f};
  rc = tiledb_array_put_metadata(ctx_, array, "bb", TILEDB_FLOAT32, 2, f);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  REQUIRE(rc == TILEDB_OK);

  // Read
  const void* v_r;
  tiledb_datatype_t v_type;
  uint32_t v_num;
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 5);

  rc = tiledb_array_get_metadata(ctx_, array, "bb", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_FLOAT32);
  CHECK(v_num == 2);
  CHECK(((const float_t*)v_r)[0] == 1.1f);
  CHECK(((const float_t*)v_r)[1] == 1.2f);

  rc = tiledb_array_get_metadata(ctx_, array, "foo", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_r == nullptr);

  uint64_t num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 2);

  const char* key;
  uint32_t key_len;
  rc = tiledb_array_get_metadata_from_index(
      ctx_, array, 10, &key, &key_len, &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_ERR);

  rc = tiledb_array_get_metadata_from_index(
      ctx_, array, 1, &key, &key_len, &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_FLOAT32);
  CHECK(v_num == 2);
  CHECK(((const float_t*)v_r)[0] == 1.1f);
  CHECK(((const float_t*)v_r)[1] == 1.2f);
  CHECK(key_len == strlen("bb"));
  CHECK(!strncmp(key, "bb", strlen("bb")));

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);
}

TEST_CASE_METHOD(
    CMetadataFx, "C API: Metadata, UTF-8", "[capi][metadata][utf-8]") {
  // Create default array
  create_default_array_1d();

  // Create and open array in write mode
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);

  // Write UTF-8 (≥ holds 3 bytes)
  int32_t v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "≥", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  REQUIRE(rc == TILEDB_OK);

  // Read
  const void* v_r;
  tiledb_datatype_t v_type;
  uint32_t v_num;
  rc = tiledb_array_get_metadata(ctx_, array, "≥", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 5);

  const char* key;
  uint32_t key_len;
  rc = tiledb_array_get_metadata_from_index(
      ctx_, array, 0, &key, &key_len, &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 5);
  CHECK(key_len == strlen("≥"));
  CHECK(!strncmp(key, "≥", strlen("≥")));

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);
}

TEST_CASE_METHOD(
    CMetadataFx, "C API: Metadata, delete", "[capi][metadata][delete]") {
  // Create default array
  create_default_array_1d();

  // Create and open array in write mode
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);

  // Write items
  int32_t v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  float f[] = {1.1f, 1.2f};
  rc = tiledb_array_put_metadata(ctx_, array, "bb", TILEDB_FLOAT32, 2, f);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Prevent array metadata filename/timestamp conflicts
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Delete an item that exists and one that does not exist
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_delete_metadata(ctx_, array, "aaa");
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_delete_metadata(ctx_, array, "foo");
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  REQUIRE(rc == TILEDB_OK);

  // Read
  const void* v_r;
  tiledb_datatype_t v_type;
  uint32_t v_num;
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_r == nullptr);

  rc = tiledb_array_get_metadata(ctx_, array, "bb", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_FLOAT32);
  CHECK(v_num == 2);
  CHECK(((const float_t*)v_r)[0] == 1.1f);
  CHECK(((const float_t*)v_r)[1] == 1.2f);

  rc = tiledb_array_get_metadata(ctx_, array, "foo", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_r == nullptr);

  uint64_t num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 1);

  const char* key;
  uint32_t key_len;
  rc = tiledb_array_get_metadata_from_index(
      ctx_, array, 0, &key, &key_len, &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_FLOAT32);
  CHECK(v_num == 2);
  CHECK(((const float_t*)v_r)[0] == 1.1f);
  CHECK(((const float_t*)v_r)[1] == 1.2f);
  CHECK(key_len == strlen("bb"));
  CHECK(!strncmp(key, "bb", strlen("bb")));

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);
}

TEST_CASE_METHOD(
    CMetadataFx,
    "C API: Metadata, multiple metadata and cosnolidate",
    "[capi][metadata][multiple][consolidation]") {
  // Create default array
  create_default_array_1d();

  // Create and open array in write mode
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);

  // Write items
  int32_t v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  float f[] = {1.1f, 1.2f};
  rc = tiledb_array_put_metadata(ctx_, array, "bb", TILEDB_FLOAT32, 2, f);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Prevent array metadata filename/timestamp conflicts
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Update
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_delete_metadata(ctx_, array, "aaa");
  CHECK(rc == TILEDB_OK);
  v = 10;
  rc = tiledb_array_put_metadata(ctx_, array, "cccc", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  REQUIRE(rc == TILEDB_OK);

  // Read
  const void* v_r;
  tiledb_datatype_t v_type;
  uint32_t v_num;
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_r == nullptr);

  rc = tiledb_array_get_metadata(ctx_, array, "bb", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_FLOAT32);
  CHECK(v_num == 2);
  CHECK(((const float_t*)v_r)[0] == 1.1f);
  CHECK(((const float_t*)v_r)[1] == 1.2f);

  rc = tiledb_array_get_metadata(ctx_, array, "cccc", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 10);

  uint64_t num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 2);

  const char* key;
  uint32_t key_len;
  rc = tiledb_array_get_metadata_from_index(
      ctx_, array, 0, &key, &key_len, &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_FLOAT32);
  CHECK(v_num == 2);
  CHECK(((const float_t*)v_r)[0] == 1.1f);
  CHECK(((const float_t*)v_r)[1] == 1.2f);
  CHECK(key_len == strlen("bb"));
  CHECK(!strncmp(key, "bb", strlen("bb")));

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Consolidate
  rc = tiledb_array_consolidate_metadata(ctx_, array_name_.c_str(), nullptr);
  CHECK(rc == TILEDB_OK);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  REQUIRE(rc == TILEDB_OK);

  num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 2);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Write once more
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);

  // Write items
  v = 50;
  rc = tiledb_array_put_metadata(ctx_, array, "d", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Consolidate again
  rc = tiledb_array_consolidate_metadata(ctx_, array_name_.c_str(), nullptr);
  CHECK(rc == TILEDB_OK);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  REQUIRE(rc == TILEDB_OK);

  num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 3);

  rc = tiledb_array_get_metadata(ctx_, array, "cccc", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 10);

  rc = tiledb_array_get_metadata(ctx_, array, "d", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 50);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);
}

TEST_CASE_METHOD(
    CMetadataFx, "C API: Metadata, open at", "[capi][metadata][open-at]") {
  // Create default array
  create_default_array_1d();

  // Create and open array in write mode
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);

  // Write items
  int32_t v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  float f[] = {1.1f, 1.2f};
  rc = tiledb_array_put_metadata(ctx_, array, "bb", TILEDB_FLOAT32, 2, f);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Prevent array metadata filename/timestamp conflicts
  auto timestamp = tiledb::sm::utils::time::timestamp_now_ms();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Update
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_delete_metadata(ctx_, array, "aaa");
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Open the array in read mode at a timestamp
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open_at(ctx_, array, TILEDB_READ, timestamp);
  REQUIRE(rc == TILEDB_OK);

  // Read
  const void* v_r;
  tiledb_datatype_t v_type;
  uint32_t v_num;
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 5);

  uint64_t num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 2);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);
}

TEST_CASE_METHOD(
    CMetadataFx, "C API: Metadata, reopen", "[capi][metadata][reopen]") {
  // Create default array
  create_default_array_1d();

  // Create and open array in write mode
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);

  // Write items
  int32_t v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  float f[] = {1.1f, 1.2f};
  rc = tiledb_array_put_metadata(ctx_, array, "bb", TILEDB_FLOAT32, 2, f);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Prevent array metadata filename/timestamp conflicts
  auto timestamp = tiledb::sm::utils::time::timestamp_now_ms();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Update
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_delete_metadata(ctx_, array, "aaa");
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Open the array in read mode at a timestamp
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open_at(ctx_, array, TILEDB_READ, timestamp);
  REQUIRE(rc == TILEDB_OK);

  // Read
  const void* v_r;
  tiledb_datatype_t v_type;
  uint32_t v_num;
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 5);

  uint64_t num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 2);

  // Reopen
  rc = tiledb_array_reopen(ctx_, array);
  CHECK(rc == TILEDB_OK);

  // Read
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_r == nullptr);

  num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 1);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);
}

TEST_CASE_METHOD(
    CMetadataFx,
    "C API: Metadata, encryption",
    "[capi][metadata][encryption]") {
  // Create default array
  create_default_array_1d_with_key();

  // Create and open array in write mode
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open_with_key(
      ctx_, array, TILEDB_WRITE, enc_type_, key_, key_len_);
  REQUIRE(rc == TILEDB_OK);

  // Write items
  int32_t v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  float f[] = {1.1f, 1.2f};
  rc = tiledb_array_put_metadata(ctx_, array, "bb", TILEDB_FLOAT32, 2, f);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Prevent array metadata filename/timestamp conflicts
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Update
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open_with_key(
      ctx_, array, TILEDB_WRITE, enc_type_, key_, key_len_);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_delete_metadata(ctx_, array, "aaa");
  CHECK(rc == TILEDB_OK);
  v = 10;
  rc = tiledb_array_put_metadata(ctx_, array, "cccc", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open_with_key(
      ctx_, array, TILEDB_READ, enc_type_, key_, key_len_);
  REQUIRE(rc == TILEDB_OK);

  // Read
  const void* v_r;
  tiledb_datatype_t v_type;
  uint32_t v_num;
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_r == nullptr);

  rc = tiledb_array_get_metadata(ctx_, array, "bb", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_FLOAT32);
  CHECK(v_num == 2);
  CHECK(((const float_t*)v_r)[0] == 1.1f);
  CHECK(((const float_t*)v_r)[1] == 1.2f);

  rc = tiledb_array_get_metadata(ctx_, array, "cccc", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 10);

  uint64_t num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 2);

  const char* key;
  uint32_t key_len;
  rc = tiledb_array_get_metadata_from_index(
      ctx_, array, 0, &key, &key_len, &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_FLOAT32);
  CHECK(v_num == 2);
  CHECK(((const float_t*)v_r)[0] == 1.1f);
  CHECK(((const float_t*)v_r)[1] == 1.2f);
  CHECK(key_len == strlen("bb"));
  CHECK(!strncmp(key, "bb", strlen("bb")));

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Consolidate without key - error
  rc = tiledb_array_consolidate_metadata(ctx_, array_name_.c_str(), nullptr);
  CHECK(rc == TILEDB_ERR);

  // Consolidate with key - ok
  rc = tiledb_array_consolidate_metadata_with_key(
      ctx_, array_name_.c_str(), enc_type_, key_, key_len_, nullptr);
  CHECK(rc == TILEDB_OK);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open_with_key(
      ctx_, array, TILEDB_READ, enc_type_, key_, key_len_);
  REQUIRE(rc == TILEDB_OK);

  num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 2);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Write once more
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open_with_key(
      ctx_, array, TILEDB_WRITE, enc_type_, key_, key_len_);
  REQUIRE(rc == TILEDB_OK);

  // Write items
  v = 50;
  rc = tiledb_array_put_metadata(ctx_, array, "d", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);

  // Consolidate again
  rc = tiledb_array_consolidate_metadata_with_key(
      ctx_, array_name_.c_str(), enc_type_, key_, key_len_, nullptr);
  CHECK(rc == TILEDB_OK);

  // Open the array in read mode
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  REQUIRE(rc == TILEDB_OK);
  rc = tiledb_array_open_with_key(
      ctx_, array, TILEDB_READ, enc_type_, key_, key_len_);
  REQUIRE(rc == TILEDB_OK);

  num = 0;
  rc = tiledb_array_get_metadata_num(ctx_, array, &num);
  CHECK(rc == TILEDB_OK);
  CHECK(num == 3);

  rc = tiledb_array_get_metadata(ctx_, array, "cccc", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 10);

  rc = tiledb_array_get_metadata(ctx_, array, "d", &v_type, &v_num, &v_r);
  CHECK(rc == TILEDB_OK);
  CHECK(v_type == TILEDB_INT32);
  CHECK(v_num == 1);
  CHECK(*((const int32_t*)v_r) == 50);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  REQUIRE(rc == TILEDB_OK);
  tiledb_array_free(&array);
}

TEST_CASE_METHOD(
    CMetadataFx, "C API: Metadata, overwrite", "[capi][metadata][overwrite]") {
  // Create default array
  create_default_array_1d();

  // Open array
  tiledb_array_t* array;
  int rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  CHECK(rc == TILEDB_OK);

  // Write and overwrite
  int32_t v = 5;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v);
  CHECK(rc == TILEDB_OK);
  int32_t v2 = 10;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v2);
  CHECK(rc == TILEDB_OK);

  // Close array
  rc = tiledb_array_close(ctx_, array);
  CHECK(rc == TILEDB_OK);

  // Read back
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  CHECK(rc == TILEDB_OK);
  const void* vback_ptr = NULL;
  tiledb_datatype_t vtype;
  uint32_t vnum = 0;
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &vtype, &vnum, &vback_ptr);
  CHECK(rc == TILEDB_OK);
  CHECK(vtype == TILEDB_INT32);
  CHECK(vnum == 1);
  CHECK(*((int32_t*)vback_ptr) == 10);
  rc = tiledb_array_close(ctx_, array);
  CHECK(rc == TILEDB_OK);

  // Prevent array metadata filename/timestamp conflicts
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Overwrite again
  rc = tiledb_array_alloc(ctx_, array_name_.c_str(), &array);
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_open(ctx_, array, TILEDB_WRITE);
  CHECK(rc == TILEDB_OK);
  int32_t v3 = 20;
  rc = tiledb_array_put_metadata(ctx_, array, "aaa", TILEDB_INT32, 1, &v3);
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_close(ctx_, array);
  CHECK(rc == TILEDB_OK);

  // Read back
  rc = tiledb_array_open(ctx_, array, TILEDB_READ);
  CHECK(rc == TILEDB_OK);
  rc = tiledb_array_get_metadata(ctx_, array, "aaa", &vtype, &vnum, &vback_ptr);
  CHECK(rc == TILEDB_OK);
  CHECK(vtype == TILEDB_INT32);
  CHECK(vnum == 1);
  CHECK(*((int32_t*)vback_ptr) == 20);
  rc = tiledb_array_close(ctx_, array);
  CHECK(rc == TILEDB_OK);
}