# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite email utility functions."""

from __future__ import print_function

import cStringIO
from email.mime.application import MIMEApplication
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
import gzip
import logging
import smtplib
import socket
import sys

from chromite.lib import cros_build_lib
from chromite.lib import retry_util


# Note: When importing this module from cbuildbot code that will run on
# a builder in the golo, set this to constants.GOLO_SMTP_SERVER
DEFAULT_SMTP_SERVER = 'localhost'

# Retry parameters for the actual smtp connection.
SMTP_RETRY_COUNT = 3
SMTP_RETRY_DELAY = 30


def _SendEmailHelper(smtp_server, sender, email_recipients, msg):
  """Send an email.

  If we get a socket error (e.g. the SMTP server is not listening or timesout),
  we will retry a few times.  All socket errors will be caught here.

  Args:
    smtp_server: The server with which to send the message.
    sender: The from address of the sender of the email.
    email_recipients: The recipients of the message.
    msg: A MIMEMultipart() object containing the body of the message.

  Returns:
    True if the email was sent, else False
  """
  def _Send():
    smtp_client = smtplib.SMTP(smtp_server)
    smtp_client.sendmail(sender, email_recipients, msg.as_string())
    smtp_client.quit()
  try:
    retry_util.RetryException(socket.error, SMTP_RETRY_COUNT, _Send,
                              sleep=SMTP_RETRY_DELAY)
    return True
  except socket.error as e:
    logging.warning('could not send e-mail from %s to %r via %r: %s',
                    sender, email_recipients, smtp_server, e)
    return False


def SendEmail(subject, recipients, smtp_server=None, message='',
              attachment=None, extra_fields=None):
  """Send an e-mail job notification with the given message in the body.

  Args:
    subject: E-mail subject.
    recipients: List of e-mail recipients.
    smtp_server: (optional) Hostname[:port] of smtp server to use to send
                 email. Defaults to DEFAULT_SMTP_SERVER.
    message: (optional) Message to put in the e-mail body.
    attachment: (optional) text to attach.
    extra_fields: (optional) A dictionary of additional message header fields
                  to be added to the message. Custom field names should begin
                  with the prefix 'X-'.
  """
  # Ignore if the list of recipients is empty.
  if not recipients:
    return

  extra_fields = extra_fields or {}

  if not smtp_server:
    smtp_server = DEFAULT_SMTP_SERVER

  sender = socket.getfqdn()
  email_recipients = recipients
  msg = MIMEMultipart()
  for key, val in extra_fields.iteritems():
    msg[key] = val
  msg['From'] = sender
  msg['Subject'] = subject
  msg['To'] = ', '.join(recipients)

  msg.attach(MIMEText(message))
  if attachment:
    s = cStringIO.StringIO()
    with gzip.GzipFile(fileobj=s, mode='w') as f:
      f.write(attachment)
    part = MIMEApplication(s.getvalue(), _subtype='x-gzip')
    s.close()
    part.add_header('Content-Disposition', 'attachment', filename='logs.txt.gz')
    msg.attach(part)

  _SendEmailHelper(smtp_server, sender, email_recipients, msg)


def SendEmailLog(subject, recipients, smtp_server=None, message='',
                 inc_trace=True, log=None, extra_fields=None):
  """Send an e-mail with a stack trace and log snippets.

  Args:
    subject: E-mail subject.
    recipients: list of e-mail recipients.
    smtp_server: (optional) Hostname(:port) of smtp server to use to send
        email. Defaults to DEFAULT_SMTP_SERVER.
    message: Message to put at the top of the e-mail body.
    inc_trace: Append a backtrace of the current stack.
    log: List of lines (log data) to include in the notice.
    extra_fields: (optional) A dictionary of additional message header fields
                  to be added to the message. Custom fields names should begin
                  with the prefix 'X-'.
  """
  if not message:
    message = subject
  message = message[:]

  if inc_trace:
    if sys.exc_info() != (None, None, None):
      trace = cros_build_lib.FormatDetailedTraceback()
      message += '\n\n' + trace

  attachment = None
  if log:
    message += ('\n\n' +
                '***************************\n' +
                'Last log messages:\n' +
                '***************************\n' +
                ''.join(log[-50:]))
    attachment = ''.join(log)

  SendEmail(subject, recipients, smtp_server, message=message,
            attachment=attachment, extra_fields=extra_fields)
