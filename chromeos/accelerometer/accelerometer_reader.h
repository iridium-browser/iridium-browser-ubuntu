// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_
#define CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "chromeos/chromeos_export.h"

template <typename T>
struct DefaultSingletonTraits;

namespace base {
class TaskRunner;
}

namespace chromeos {

// Reads an accelerometer device and reports data back to an
// AccelerometerDelegate.
class CHROMEOS_EXPORT AccelerometerReader {
 public:
  // The time to wait between reading the accelerometer.
  static const int kDelayBetweenReadsMs;

  // Configuration structure for accelerometer device.
  struct ConfigurationData {
    ConfigurationData();
    ~ConfigurationData();

    // Number of accelerometers on device.
    size_t count;

    // Length of accelerometer updates.
    size_t length;

    // Which accelerometers are present on device.
    bool has[ACCELEROMETER_SOURCE_COUNT];

    // Scale of accelerometers (i.e. raw value * scale = m/s^2).
    float scale[ACCELEROMETER_SOURCE_COUNT][3];

    // Index of each accelerometer axis in data stream.
    int index[ACCELEROMETER_SOURCE_COUNT][3];
  };
  typedef base::RefCountedData<ConfigurationData> Configuration;
  typedef base::RefCountedData<char[12]> Reading;

  // An interface to receive data from the AccelerometerReader.
  class Observer {
   public:
    virtual void OnAccelerometerUpdated(
        scoped_refptr<const AccelerometerUpdate> update) = 0;

   protected:
    virtual ~Observer() {}
  };

  static AccelerometerReader* GetInstance();

  void Initialize(scoped_refptr<base::TaskRunner> blocking_task_runner);

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  AccelerometerReader();
  virtual ~AccelerometerReader();

 private:
  friend struct DefaultSingletonTraits<AccelerometerReader>;

  // Dispatched when initialization is complete. If |success|, |configuration|
  // provides the details of the detected accelerometer.
  void OnInitialized(scoped_refptr<Configuration> configuration, bool success);

  // Triggers an asynchronous read from the accelerometer, signalling
  // OnDataRead with the result.
  void TriggerRead();

  // If |success|, converts the raw reading to an AccelerometerUpdate
  // message and notifies the |delegate_| with the new readings.
  // Triggers another read from the accelerometer at the current sampling rate.
  void OnDataRead(scoped_refptr<Reading> reading, bool success);

  // The task runner to use for blocking tasks.
  scoped_refptr<base::TaskRunner> task_runner_;

  // The last seen accelerometer data.
  scoped_refptr<AccelerometerUpdate> update_;

  // The accelerometer configuration.
  scoped_refptr<Configuration> configuration_;

  scoped_refptr<ObserverListThreadSafe<Observer>> observers_;

  base::WeakPtrFactory<AccelerometerReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccelerometerReader);
};

}  // namespace chromeos

#endif  // CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_
