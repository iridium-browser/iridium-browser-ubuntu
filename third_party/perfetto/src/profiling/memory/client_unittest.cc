/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/profiling/memory/client.h"

#include "gtest/gtest.h"
#include "perfetto/base/unix_socket.h"

#include <thread>

namespace perfetto {
namespace profiling {
namespace {

base::UnixSocketRaw CreateSocket() {
  auto sock = base::UnixSocketRaw::CreateMayFail(base::SockType::kStream);
  PERFETTO_CHECK(sock);
  return sock;
}

TEST(SocketPoolTest, Basic) {
  std::vector<base::UnixSocketRaw> socks;
  socks.emplace_back(CreateSocket());
  SocketPool pool(std::move(socks));
  BorrowedSocket sock = pool.Borrow();
}

TEST(SocketPoolTest, Close) {
  std::vector<base::UnixSocketRaw> socks;
  socks.emplace_back(CreateSocket());
  SocketPool pool(std::move(socks));
  BorrowedSocket sock = pool.Borrow();
  sock.Shutdown();
}

TEST(SocketPoolTest, Multiple) {
  std::vector<base::UnixSocketRaw> socks;
  socks.emplace_back(CreateSocket());
  socks.emplace_back(CreateSocket());
  SocketPool pool(std::move(socks));
  BorrowedSocket sock = pool.Borrow();
  BorrowedSocket sock_2 = pool.Borrow();
}

TEST(SocketPoolTest, Blocked) {
  std::vector<base::UnixSocketRaw> socks;
  socks.emplace_back(CreateSocket());
  SocketPool pool(std::move(socks));
  BorrowedSocket sock = pool.Borrow();  // Takes the socket above.
  std::thread t([&pool] { pool.Borrow(); });
  {
    // Return fd to unblock thread.
    BorrowedSocket temp = std::move(sock);
  }
  t.join();
}

TEST(SocketPoolTest, BlockedClose) {
  std::vector<base::UnixSocketRaw> socks;
  socks.emplace_back(CreateSocket());
  SocketPool pool(std::move(socks));
  BorrowedSocket sock = pool.Borrow();
  std::thread t([&pool] { pool.Borrow(); });
  {
    // Return fd to unblock thread.
    BorrowedSocket temp = std::move(sock);
    temp.Shutdown();
  }
  t.join();
}

TEST(SocketPoolTest, MultipleBlocked) {
  std::vector<base::UnixSocketRaw> socks;
  socks.emplace_back(CreateSocket());
  SocketPool pool(std::move(socks));
  BorrowedSocket sock = pool.Borrow();
  std::thread t([&pool] { pool.Borrow(); });
  std::thread t2([&pool] { pool.Borrow(); });
  {
    // Return fd to unblock thread.
    BorrowedSocket temp = std::move(sock);
  }
  t.join();
  t2.join();
}

TEST(SocketPoolTest, MultipleBlockedClose) {
  std::vector<base::UnixSocketRaw> socks;
  socks.emplace_back(CreateSocket());
  SocketPool pool(std::move(socks));
  BorrowedSocket sock = pool.Borrow();
  std::thread t([&pool] { pool.Borrow(); });
  std::thread t2([&pool] { pool.Borrow(); });
  {
    // Return fd to unblock thread.
    BorrowedSocket temp = std::move(sock);
    temp.Shutdown();
  }
  t.join();
  t2.join();
}

TEST(FreePageTest, ShutdownSocketPool) {
  std::vector<base::UnixSocketRaw> socks;
  socks.emplace_back(CreateSocket());
  SocketPool pool(std::move(socks));
  pool.Shutdown();
  FreePage p;
  p.Add(0, 1, &pool);
}

TEST(ClientTest, GetThreadStackBase) {
  std::thread th([] {
    const char* stackbase = GetThreadStackBase();
    ASSERT_NE(stackbase, nullptr);
    // The implementation assumes the stack grows from higher addresses to
    // lower. We will need to rework once we encounter architectures where the
    // stack grows the other way.
    EXPECT_GT(stackbase, __builtin_frame_address(0));
  });
  th.join();
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
