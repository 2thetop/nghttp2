/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef SHRPX_MEMCACHED_CONNECTION_H
#define SHRPX_MEMCACHED_CONNECTION_H

#include "shrpx.h"

#include <memory>
#include <deque>

#include <ev.h>

#include "shrpx_connection.h"
#include "shrpx_ssl.h"
#include "buffer.h"
#include "network.h"

using namespace nghttp2;

namespace shrpx {

struct MemcachedRequest;

enum {
  MEMCACHED_PARSE_HEADER24,
  MEMCACHED_PARSE_EXTRA,
  MEMCACHED_PARSE_VALUE,
};

// Stores state when parsing response from memcached server
struct MemcachedParseState {
  // Buffer for value, dynamically allocated.
  std::vector<uint8_t> value;
  // cas in response
  uint64_t cas;
  // keylen in response
  size_t keylen;
  // extralen in response
  size_t extralen;
  // totalbody in response.  The length of value is totalbody -
  // extralen - keylen.
  size_t totalbody;
  // Number of bytes left to read variable length field.
  size_t read_left;
  // Parser state; see enum above
  int state;
  // status_code in response
  int status_code;
  // op in response
  int op;
};

struct MemcachedSendbuf {
  // Buffer for header + extra + key
  Buffer<512> headbuf;
  // MemcachedRequest associated to this object
  MemcachedRequest *req;
  // Number of bytes left when sending value
  size_t send_value_left;
  // Returns the number of bytes this object transmits.
  size_t left() const { return headbuf.rleft() + send_value_left; }
};

constexpr uint8_t MEMCACHED_REQ_MAGIC = 0x80;
constexpr uint8_t MEMCACHED_RES_MAGIC = 0x81;

// MemcachedConnection implements part of memcached binary protocol.
// This is not full brown implementation.  Just the part we need is
// implemented.  We only use GET and ADD.
//
// https://github.com/memcached/memcached/blob/master/doc/protocol-binary.xml
// https://code.google.com/p/memcached/wiki/MemcacheBinaryProtocol
class MemcachedConnection {
public:
  MemcachedConnection(const Address *addr, struct ev_loop *loop,
                      SSL_CTX *ssl_ctx, const StringRef &sni_name,
                      MemchunkPool *mcpool);
  ~MemcachedConnection();

  void disconnect();

  int add_request(std::unique_ptr<MemcachedRequest> req);
  int initiate_connection();

  int connected();
  int on_write();
  int on_read();

  int write_clear();
  int read_clear();

  int tls_handshake();
  int write_tls();
  int read_tls();

  size_t fill_request_buffer(struct iovec *iov, size_t iovlen);
  void drain_send_queue(size_t nwrite);

  void make_request(MemcachedSendbuf *sendbuf, MemcachedRequest *req);
  int parse_packet();
  size_t serialized_size(MemcachedRequest *req);

  void signal_write();

  int noop();

private:
  Connection conn_;
  std::deque<std::unique_ptr<MemcachedRequest>> recvq_;
  std::deque<std::unique_ptr<MemcachedRequest>> sendq_;
  std::deque<MemcachedSendbuf> sendbufv_;
  std::function<int(MemcachedConnection &)> do_read_, do_write_;
  std::string sni_name_;
  ssl::TLSSessionCache tls_session_cache_;
  MemcachedParseState parse_state_;
  const Address *addr_;
  SSL_CTX *ssl_ctx_;
  // Sum of the bytes to be transmitted in sendbufv_.
  size_t sendsum_;
  bool connected_;
  Buffer<8_k> recvbuf_;
};

} // namespace shrpx

#endif // SHRPX_MEMCACHED_CONNECTION_H
