// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package bindings

import (
	"fmt"

	"mojo/public/go/system"
)

const (
	// Flag for a header of a simple message.
	MessageNoFlag = 0

	// Flag for a header of a message that expected a response.
	MessageExpectsResponseFlag = 1 << 0

	// Flag for a header of a message that is a response.
	MessageIsResponseFlag = 1 << 1

	dataHeaderSize   = 8
	defaultAlignment = 8
	pointerBitSize   = 64
)

var mapHeader DataHeader

func init() {
	mapHeader = DataHeader{24, 2}
}

// Payload is an interface implemented by a mojo struct that can encode/decode
// itself into mojo archive format.
type Payload interface {
	Encode(encoder *Encoder) error
	Decode(decoder *Decoder) error
}

// DataHeader is a header for a mojo complex element.
type DataHeader struct {
	Size              uint32
	ElementsOrVersion uint32
}

// MessageHeader is a header information for a message.
type MessageHeader struct {
	Type      uint32
	Flags     uint32
	RequestId uint64
}

func (h *MessageHeader) Encode(encoder *Encoder) error {
	encoder.StartStruct(h.dataSize(), h.numFields())
	if err := encoder.WriteUint32(h.Type); err != nil {
		return err
	}
	if err := encoder.WriteUint32(h.Flags); err != nil {
		return err
	}
	if h.RequestId != 0 {
		if err := encoder.WriteUint64(h.RequestId); err != nil {
			return err
		}
	}
	return encoder.Finish()
}

func (h *MessageHeader) Decode(decoder *Decoder) error {
	header, err := decoder.StartStruct()
	if err != nil {
		return err
	}
	numFields := header.ElementsOrVersion
	if numFields < 2 || numFields > 3 {
		return fmt.Errorf("Invalid message header: it should have 2 or 3 fileds, but has %d", numFields)
	}
	if h.Type, err = decoder.ReadUint32(); err != nil {
		return err
	}
	if h.Flags, err = decoder.ReadUint32(); err != nil {
		return err
	}
	if numFields == 3 {
		if h.Flags != MessageExpectsResponseFlag && h.Flags != MessageIsResponseFlag {
			return fmt.Errorf("Message header flags(%v) should be MessageExpectsResponseFlag or MessageIsResponseFlag", h.Flags)
		}
		if h.RequestId, err = decoder.ReadUint64(); err != nil {
			return err
		}
	} else {
		if h.Flags != MessageNoFlag {
			return fmt.Errorf("Message header flags(%v) should be MessageNoFlag", h.Flags)
		}
	}
	return decoder.Finish()
}

func (h *MessageHeader) dataSize() uint32 {
	var size uint32
	size = 2 * 4
	if h.RequestId != 0 {
		size += 8
	}
	return size
}

func (h *MessageHeader) numFields() uint32 {
	if h.RequestId != 0 {
		return 3
	} else {
		return 2
	}
}

// Message is a a raw message to be sent/received from a message pipe handle
// which contains a message header.
type Message struct {
	Header  MessageHeader
	Bytes   []byte
	Handles []system.UntypedHandle
	Payload []byte
}

func newMessage(header MessageHeader, bytes []byte, handles []system.UntypedHandle) *Message {
	return &Message{header, bytes, handles, bytes[header.dataSize()+dataHeaderSize:]}
}

// DecodePayload decodes the provided payload from the message.
func (m *Message) DecodePayload(payload Payload) error {
	decoder := NewDecoder(m.Payload, m.Handles)
	if err := payload.Decode(decoder); err != nil {
		return err
	}
	return nil
}

// EncodeMessage returns a message with provided header that has provided
// payload encoded in mojo archive format.
func EncodeMessage(header MessageHeader, payload Payload) (*Message, error) {
	encoder := NewEncoder()
	if err := header.Encode(encoder); err != nil {
		return nil, err
	}
	if err := payload.Encode(encoder); err != nil {
		return nil, err
	}
	if bytes, handles, err := encoder.Data(); err != nil {
		return nil, err
	} else {
		return newMessage(header, bytes, handles), nil
	}
}

// ParseMessage parses message header from byte buffer with attached handles
// and returnes parsed message.
func ParseMessage(bytes []byte, handles []system.UntypedHandle) (*Message, error) {
	decoder := NewDecoder(bytes, []system.UntypedHandle{})
	var header MessageHeader
	if err := header.Decode(decoder); err != nil {
		return nil, err
	}
	return newMessage(header, bytes, handles), nil
}
