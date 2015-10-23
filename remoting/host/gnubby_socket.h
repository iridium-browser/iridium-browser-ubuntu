// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_GNUBBY_SOCKET_H_
#define REMOTING_HOST_GNUBBY_SOCKET_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace base {
class Timer;
}  // namespace base

namespace net {
class DrainableIOBuffer;
class IOBufferWithSize;
class StreamSocket;
}  // namespace net

namespace remoting {

// Class that manages reading requests and sending responses. The socket can
// only handle receiving one request at a time. It expects to receive no extra
// bytes over the wire, which is checked by IsRequestTooLarge method.
class GnubbySocket : public base::NonThreadSafe {
 public:
  GnubbySocket(scoped_ptr<net::StreamSocket> socket,
               const base::TimeDelta& timeout,
               const base::Closure& timeout_callback);
  ~GnubbySocket();

  // Returns false if the request has not yet completed, or is too large to be
  // processed. Otherwise, the cached request data is copied into |data_out| and
  // the internal buffer resets and is ready for the next request.
  bool GetAndClearRequestData(std::string* data_out);

  // Sends response data to the socket.
  void SendResponse(const std::string& data);

  // Sends an SSH error code to the socket.
  void SendSshError();

  // |request_received_callback| is used to notify the caller that request data
  // has been fully read, and caller is to use GetAndClearRequestData method to
  // get the request data.
  void StartReadingRequest(const base::Closure& request_received_callback);

 private:
  // Called when bytes are written to |socket_|.
  void OnDataWritten(int result);

  // Continues writing to |socket_| if needed.
  void DoWrite();

  // Called when bytes are read from |socket_|.
  void OnDataRead(int bytes_read);

  // Continues to read.
  void DoRead();

  // Returns true if the current request is complete.
  bool IsRequestComplete() const;

  // Returns true if the stated request size is larger than the allowed maximum.
  bool IsRequestTooLarge() const;

  // Returns the stated request length.
  size_t GetRequestLength() const;

  // Returns the response length bytes.
  std::string GetResponseLengthAsBytes(const std::string& response) const;

  // Resets the socket activity timer.
  void ResetTimer();

  // The socket.
  scoped_ptr<net::StreamSocket> socket_;

  // Invoked when request data has been read.
  base::Closure request_received_callback_;

  // Indicates whether read has completed and |request_received_callback_| is
  // about to be run.
  bool read_completed_;

  // Request data.
  std::vector<char> request_data_;

  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  scoped_refptr<net::IOBufferWithSize> read_buffer_;

  // The activity timer.
  scoped_ptr<base::Timer> timer_;

  DISALLOW_COPY_AND_ASSIGN(GnubbySocket);
};

}  // namespace remoting

#endif  // REMOTING_HOST_GNUBBY_SOCKET_H_
