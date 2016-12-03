# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections


PortPair = collections.namedtuple('PortPair', ['local_port', 'remote_port'])
PortSet = collections.namedtuple('PortSet', ['http', 'https', 'dns'])

class PortPairs(collections.namedtuple('PortPairs', ['http', 'https', 'dns'])):
  __slots__ = ()

  @classmethod
  def Zip(cls, local_ports, remote_ports):
    """Zip a pair of PortSet's into a single PortPairs object."""
    with_dns = local_ports.dns is not None and remote_ports.dns is not None
    return cls(
      PortPair(local_ports.http, remote_ports.http),
      PortPair(local_ports.https, remote_ports.https),
      PortPair(local_ports.dns, remote_ports.dns) if with_dns else None)

  @property
  def local_ports(self):
    """Return a tuple of local ports only."""
    return PortSet(*[p.local_port if p is not None else None for p in self])

  @property
  def remote_ports(self):
    """Return a tuple of remote ports only."""
    return PortSet(*[p.remote_port if p is not None else None for p in self])


class ForwarderFactory(object):

  def Create(self, port_pairs):
    """Creates a forwarder that maps remote (device) <-> local (host) ports.

    Args:
      port_pairs: A PortPairs instance that consists of a PortPair mapping
          for each protocol. http is required. https and dns may be None.
    """
    raise NotImplementedError()

  @property
  def host_ip(self):
    return '127.0.0.1'


class Forwarder(object):

  def __init__(self, port_pairs):
    assert port_pairs.http, 'HTTP port mapping is required.'
    self._port_pairs = port_pairs
    self._forwarding = True

  @property
  def host_port(self):
    return self._port_pairs.http.remote_port

  @property
  def host_ip(self):
    return '127.0.0.1'

  @property
  def port_pairs(self):
    return self._port_pairs

  @property
  def url(self):
    assert self.host_ip and self.host_port
    return 'http://%s:%i' % (self.host_ip, self.host_port)

  def Close(self):
    self._port_pairs = None
    self._forwarding = False
