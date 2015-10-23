// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_connection.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/api/serial.h"

namespace extensions {

namespace {

const int kDefaultBufferSize = 4096;

api::serial::SendError ConvertSendErrorFromMojo(
    device::serial::SendError input) {
  switch (input) {
    case device::serial::SEND_ERROR_NONE:
      return api::serial::SEND_ERROR_NONE;
    case device::serial::SEND_ERROR_DISCONNECTED:
      return api::serial::SEND_ERROR_DISCONNECTED;
    case device::serial::SEND_ERROR_PENDING:
      return api::serial::SEND_ERROR_PENDING;
    case device::serial::SEND_ERROR_TIMEOUT:
      return api::serial::SEND_ERROR_TIMEOUT;
    case device::serial::SEND_ERROR_SYSTEM_ERROR:
      return api::serial::SEND_ERROR_SYSTEM_ERROR;
  }
  return api::serial::SEND_ERROR_NONE;
}

api::serial::ReceiveError ConvertReceiveErrorFromMojo(
    device::serial::ReceiveError input) {
  switch (input) {
    case device::serial::RECEIVE_ERROR_NONE:
      return api::serial::RECEIVE_ERROR_NONE;
    case device::serial::RECEIVE_ERROR_DISCONNECTED:
      return api::serial::RECEIVE_ERROR_DISCONNECTED;
    case device::serial::RECEIVE_ERROR_TIMEOUT:
      return api::serial::RECEIVE_ERROR_TIMEOUT;
    case device::serial::RECEIVE_ERROR_DEVICE_LOST:
      return api::serial::RECEIVE_ERROR_DEVICE_LOST;
    case device::serial::RECEIVE_ERROR_BREAK:
      return api::serial::RECEIVE_ERROR_BREAK;
    case device::serial::RECEIVE_ERROR_FRAME_ERROR:
      return api::serial::RECEIVE_ERROR_FRAME_ERROR;
    case device::serial::RECEIVE_ERROR_OVERRUN:
      return api::serial::RECEIVE_ERROR_OVERRUN;
    case device::serial::RECEIVE_ERROR_BUFFER_OVERFLOW:
      return api::serial::RECEIVE_ERROR_BUFFER_OVERFLOW;
    case device::serial::RECEIVE_ERROR_PARITY_ERROR:
      return api::serial::RECEIVE_ERROR_PARITY_ERROR;
    case device::serial::RECEIVE_ERROR_SYSTEM_ERROR:
      return api::serial::RECEIVE_ERROR_SYSTEM_ERROR;
  }
  return api::serial::RECEIVE_ERROR_NONE;
}

api::serial::DataBits ConvertDataBitsFromMojo(device::serial::DataBits input) {
  switch (input) {
    case device::serial::DATA_BITS_NONE:
      return api::serial::DATA_BITS_NONE;
    case device::serial::DATA_BITS_SEVEN:
      return api::serial::DATA_BITS_SEVEN;
    case device::serial::DATA_BITS_EIGHT:
      return api::serial::DATA_BITS_EIGHT;
  }
  return api::serial::DATA_BITS_NONE;
}

device::serial::DataBits ConvertDataBitsToMojo(api::serial::DataBits input) {
  switch (input) {
    case api::serial::DATA_BITS_NONE:
      return device::serial::DATA_BITS_NONE;
    case api::serial::DATA_BITS_SEVEN:
      return device::serial::DATA_BITS_SEVEN;
    case api::serial::DATA_BITS_EIGHT:
      return device::serial::DATA_BITS_EIGHT;
  }
  return device::serial::DATA_BITS_NONE;
}

api::serial::ParityBit ConvertParityBitFromMojo(
    device::serial::ParityBit input) {
  switch (input) {
    case device::serial::PARITY_BIT_NONE:
      return api::serial::PARITY_BIT_NONE;
    case device::serial::PARITY_BIT_ODD:
      return api::serial::PARITY_BIT_ODD;
    case device::serial::PARITY_BIT_NO:
      return api::serial::PARITY_BIT_NO;
    case device::serial::PARITY_BIT_EVEN:
      return api::serial::PARITY_BIT_EVEN;
  }
  return api::serial::PARITY_BIT_NONE;
}

device::serial::ParityBit ConvertParityBitToMojo(api::serial::ParityBit input) {
  switch (input) {
    case api::serial::PARITY_BIT_NONE:
      return device::serial::PARITY_BIT_NONE;
    case api::serial::PARITY_BIT_NO:
      return device::serial::PARITY_BIT_NO;
    case api::serial::PARITY_BIT_ODD:
      return device::serial::PARITY_BIT_ODD;
    case api::serial::PARITY_BIT_EVEN:
      return device::serial::PARITY_BIT_EVEN;
  }
  return device::serial::PARITY_BIT_NONE;
}

api::serial::StopBits ConvertStopBitsFromMojo(device::serial::StopBits input) {
  switch (input) {
    case device::serial::STOP_BITS_NONE:
      return api::serial::STOP_BITS_NONE;
    case device::serial::STOP_BITS_ONE:
      return api::serial::STOP_BITS_ONE;
    case device::serial::STOP_BITS_TWO:
      return api::serial::STOP_BITS_TWO;
  }
  return api::serial::STOP_BITS_NONE;
}

device::serial::StopBits ConvertStopBitsToMojo(api::serial::StopBits input) {
  switch (input) {
    case api::serial::STOP_BITS_NONE:
      return device::serial::STOP_BITS_NONE;
    case api::serial::STOP_BITS_ONE:
      return device::serial::STOP_BITS_ONE;
    case api::serial::STOP_BITS_TWO:
      return device::serial::STOP_BITS_TWO;
  }
  return device::serial::STOP_BITS_NONE;
}

class SendBuffer : public device::ReadOnlyBuffer {
 public:
  SendBuffer(
      const std::vector<char>& data,
      const base::Callback<void(int, device::serial::SendError)>& callback)
      : data_(data), callback_(callback) {}
  ~SendBuffer() override {}
  const char* GetData() override { return vector_as_array(&data_); }
  uint32_t GetSize() override { return static_cast<uint32_t>(data_.size()); }
  void Done(uint32_t bytes_read) override {
    callback_.Run(bytes_read, device::serial::SEND_ERROR_NONE);
  }
  void DoneWithError(uint32_t bytes_read, int32_t error) override {
    callback_.Run(bytes_read, static_cast<device::serial::SendError>(error));
  }

 private:
  const std::vector<char> data_;
  const base::Callback<void(int, device::serial::SendError)> callback_;
};

class ReceiveBuffer : public device::WritableBuffer {
 public:
  ReceiveBuffer(
      scoped_refptr<net::IOBuffer> buffer,
      uint32_t size,
      const base::Callback<void(int, device::serial::ReceiveError)>& callback)
      : buffer_(buffer), size_(size), callback_(callback) {}
  ~ReceiveBuffer() override {}
  char* GetData() override { return buffer_->data(); }
  uint32_t GetSize() override { return size_; }
  void Done(uint32_t bytes_written) override {
    callback_.Run(bytes_written, device::serial::RECEIVE_ERROR_NONE);
  }
  void DoneWithError(uint32_t bytes_written, int32_t error) override {
    callback_.Run(bytes_written,
                  static_cast<device::serial::ReceiveError>(error));
  }

 private:
  scoped_refptr<net::IOBuffer> buffer_;
  const uint32_t size_;
  const base::Callback<void(int, device::serial::ReceiveError)> callback_;
};

}  // namespace

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ApiResourceManager<SerialConnection> > >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<SerialConnection> >*
ApiResourceManager<SerialConnection>::GetFactoryInstance() {
  return g_factory.Pointer();
}

SerialConnection::SerialConnection(const std::string& port,
                                   const std::string& owner_extension_id)
    : ApiResource(owner_extension_id),
      port_(port),
      persistent_(false),
      buffer_size_(kDefaultBufferSize),
      receive_timeout_(0),
      send_timeout_(0),
      paused_(false),
      io_handler_(device::SerialIoHandler::Create(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::FILE),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::UI))) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

SerialConnection::~SerialConnection() {
  io_handler_->CancelRead(device::serial::RECEIVE_ERROR_DISCONNECTED);
  io_handler_->CancelWrite(device::serial::SEND_ERROR_DISCONNECTED);
}

bool SerialConnection::IsPersistent() const {
  return persistent();
}

void SerialConnection::set_buffer_size(int buffer_size) {
  buffer_size_ = buffer_size;
}

void SerialConnection::set_receive_timeout(int receive_timeout) {
  receive_timeout_ = receive_timeout;
}

void SerialConnection::set_send_timeout(int send_timeout) {
  send_timeout_ = send_timeout;
}

void SerialConnection::set_paused(bool paused) {
  paused_ = paused;
  if (paused) {
    io_handler_->CancelRead(device::serial::RECEIVE_ERROR_NONE);
  }
}

void SerialConnection::Open(const api::serial::ConnectionOptions& options,
                            const OpenCompleteCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (options.persistent.get())
    set_persistent(*options.persistent);
  if (options.name.get())
    set_name(*options.name);
  if (options.buffer_size.get())
    set_buffer_size(*options.buffer_size);
  if (options.receive_timeout.get())
    set_receive_timeout(*options.receive_timeout);
  if (options.send_timeout.get())
    set_send_timeout(*options.send_timeout);
  io_handler_->Open(port_, *device::serial::ConnectionOptions::From(options),
                    callback);
}

bool SerialConnection::Receive(const ReceiveCompleteCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!receive_complete_.is_null())
    return false;
  receive_complete_ = callback;
  receive_buffer_ = new net::IOBuffer(buffer_size_);
  io_handler_->Read(scoped_ptr<device::WritableBuffer>(new ReceiveBuffer(
      receive_buffer_,
      buffer_size_,
      base::Bind(&SerialConnection::OnAsyncReadComplete, AsWeakPtr()))));
  receive_timeout_task_.reset();
  if (receive_timeout_ > 0) {
    receive_timeout_task_.reset(new TimeoutTask(
        base::Bind(&SerialConnection::OnReceiveTimeout, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(receive_timeout_)));
  }
  return true;
}

bool SerialConnection::Send(const std::vector<char>& data,
                            const SendCompleteCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!send_complete_.is_null())
    return false;
  send_complete_ = callback;
  io_handler_->Write(scoped_ptr<device::ReadOnlyBuffer>(new SendBuffer(
      data, base::Bind(&SerialConnection::OnAsyncWriteComplete, AsWeakPtr()))));
  send_timeout_task_.reset();
  if (send_timeout_ > 0) {
    send_timeout_task_.reset(new TimeoutTask(
        base::Bind(&SerialConnection::OnSendTimeout, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(send_timeout_)));
  }
  return true;
}

bool SerialConnection::Configure(
    const api::serial::ConnectionOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (options.persistent.get())
    set_persistent(*options.persistent);
  if (options.name.get())
    set_name(*options.name);
  if (options.buffer_size.get())
    set_buffer_size(*options.buffer_size);
  if (options.receive_timeout.get())
    set_receive_timeout(*options.receive_timeout);
  if (options.send_timeout.get())
    set_send_timeout(*options.send_timeout);
  bool success = io_handler_->ConfigurePort(
      *device::serial::ConnectionOptions::From(options));
  io_handler_->CancelRead(device::serial::RECEIVE_ERROR_NONE);
  return success;
}

void SerialConnection::SetIoHandlerForTest(
    scoped_refptr<device::SerialIoHandler> handler) {
  io_handler_ = handler;
}

bool SerialConnection::GetInfo(api::serial::ConnectionInfo* info) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  info->paused = paused_;
  info->persistent = persistent_;
  info->name = name_;
  info->buffer_size = buffer_size_;
  info->receive_timeout = receive_timeout_;
  info->send_timeout = send_timeout_;
  device::serial::ConnectionInfoPtr port_info = io_handler_->GetPortInfo();
  if (!port_info)
    return false;

  info->bitrate.reset(new int(port_info->bitrate));
  info->data_bits = ConvertDataBitsFromMojo(port_info->data_bits);
  info->parity_bit = ConvertParityBitFromMojo(port_info->parity_bit);
  info->stop_bits = ConvertStopBitsFromMojo(port_info->stop_bits);
  info->cts_flow_control.reset(new bool(port_info->cts_flow_control));
  return true;
}

bool SerialConnection::Flush() const {
  return io_handler_->Flush();
}

bool SerialConnection::GetControlSignals(
    api::serial::DeviceControlSignals* control_signals) const {
  device::serial::DeviceControlSignalsPtr signals =
      io_handler_->GetControlSignals();
  if (!signals)
    return false;

  control_signals->dcd = signals->dcd;
  control_signals->cts = signals->cts;
  control_signals->ri = signals->ri;
  control_signals->dsr = signals->dsr;
  return true;
}

bool SerialConnection::SetControlSignals(
    const api::serial::HostControlSignals& control_signals) {
  return io_handler_->SetControlSignals(
      *device::serial::HostControlSignals::From(control_signals));
}

bool SerialConnection::SetBreak() {
  return io_handler_->SetBreak();
}

bool SerialConnection::ClearBreak() {
  return io_handler_->ClearBreak();
}

void SerialConnection::OnReceiveTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  io_handler_->CancelRead(device::serial::RECEIVE_ERROR_TIMEOUT);
}

void SerialConnection::OnSendTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  io_handler_->CancelWrite(device::serial::SEND_ERROR_TIMEOUT);
}

void SerialConnection::OnAsyncReadComplete(int bytes_read,
                                           device::serial::ReceiveError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!receive_complete_.is_null());
  ReceiveCompleteCallback callback = receive_complete_;
  receive_complete_.Reset();
  receive_timeout_task_.reset();
  callback.Run(std::vector<char>(receive_buffer_->data(),
                                 receive_buffer_->data() + bytes_read),
               ConvertReceiveErrorFromMojo(error));
  receive_buffer_ = NULL;
}

void SerialConnection::OnAsyncWriteComplete(int bytes_sent,
                                            device::serial::SendError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!send_complete_.is_null());
  SendCompleteCallback callback = send_complete_;
  send_complete_.Reset();
  send_timeout_task_.reset();
  callback.Run(bytes_sent, ConvertSendErrorFromMojo(error));
}

SerialConnection::TimeoutTask::TimeoutTask(const base::Closure& closure,
                                           const base::TimeDelta& delay)
    : closure_(closure), delay_(delay), weak_factory_(this) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TimeoutTask::Run, weak_factory_.GetWeakPtr()),
      delay_);
}

SerialConnection::TimeoutTask::~TimeoutTask() {
}

void SerialConnection::TimeoutTask::Run() const {
  closure_.Run();
}

}  // namespace extensions

namespace mojo {

// static
device::serial::HostControlSignalsPtr
TypeConverter<device::serial::HostControlSignalsPtr,
              extensions::api::serial::HostControlSignals>::
    Convert(const extensions::api::serial::HostControlSignals& input) {
  device::serial::HostControlSignalsPtr output(
      device::serial::HostControlSignals::New());
  if (input.dtr.get()) {
    output->has_dtr = true;
    output->dtr = *input.dtr;
  }
  if (input.rts.get()) {
    output->has_rts = true;
    output->rts = *input.rts;
  }
  return output.Pass();
}

// static
device::serial::ConnectionOptionsPtr
TypeConverter<device::serial::ConnectionOptionsPtr,
              extensions::api::serial::ConnectionOptions>::
    Convert(const extensions::api::serial::ConnectionOptions& input) {
  device::serial::ConnectionOptionsPtr output(
      device::serial::ConnectionOptions::New());
  if (input.bitrate.get() && *input.bitrate > 0)
    output->bitrate = *input.bitrate;
  output->data_bits = extensions::ConvertDataBitsToMojo(input.data_bits);
  output->parity_bit = extensions::ConvertParityBitToMojo(input.parity_bit);
  output->stop_bits = extensions::ConvertStopBitsToMojo(input.stop_bits);
  if (input.cts_flow_control.get()) {
    output->has_cts_flow_control = true;
    output->cts_flow_control = *input.cts_flow_control;
  }
  return output.Pass();
}

}  // namespace mojo
