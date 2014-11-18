/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.ipc.invalidation.examples.android2;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.examples.android2.ExampleListenerProto.ExampleListenerStateProto;
import com.google.ipc.invalidation.examples.android2.ExampleListenerProto.ExampleListenerStateProto.ObjectIdProto;
import com.google.ipc.invalidation.examples.android2.ExampleListenerProto.ExampleListenerStateProto.ObjectStateProto;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import android.util.Base64;
import android.util.Log;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;


/**
 * Wrapper around persistent state for {@link ExampleListener}.
 *
 */
public class ExampleListenerState {

  /** Wrapper around persistent state for an object tracked by the {@link ExampleListener}. */
  private static class ObjectState {
    /** Object id for the object being tracked. */
    final ObjectId objectId;

    /** Indicates whether the example listener wants to be registered for this object. */
    boolean isRegistered;

    /**
     * Payload of the invalidation with the highest version received so far. {@code null} before
     * any invalidations have been received or after an unknown-version invalidation is received.
     */
    ByteString payload;

    /**
     * Highest version invalidation received so far. {@code null} before any invalidations have
     * been received or after an unknown-version invalidation is received.
     */
    Long highestVersion;

    /** Wall time in milliseconds at which most recent invalidation was received. */
    Long invalidationTimeMillis;

    /** Indicates whether the last invalidation received was a background invalidation. */
    boolean isBackground;

    ObjectState(ObjectStateProto objectStateProto) {
      objectId = deserializeObjectId(objectStateProto.getObjectId());
      isRegistered = objectStateProto.getIsRegistered();
      if (objectStateProto.hasPayload()) {
        payload = objectStateProto.getPayload();
      }
      if (objectStateProto.hasHighestVersion()) {
        highestVersion = objectStateProto.getHighestVersion();
      }
      if (objectStateProto.hasInvalidationTimeMillis()) {
        invalidationTimeMillis = objectStateProto.getInvalidationTimeMillis();
      }
      isBackground = objectStateProto.getIsBackground();
    }

    ObjectState(ObjectId objectId, boolean isRegistered) {
      this.objectId = objectId;
      this.isRegistered = isRegistered;
    }

    ObjectStateProto serialize() {
      ObjectStateProto.Builder builder = ObjectStateProto.newBuilder()
          .setObjectId(ExampleListenerState.serializeObjectId(objectId))
          .setIsRegistered(isRegistered)
          .setIsBackground(isBackground);
      if (payload != null) {
        builder.setPayload(payload);
      }
      if (highestVersion != null) {
        builder.setHighestVersion(highestVersion.longValue());
      }
      if (invalidationTimeMillis != null) {
        builder.setInvalidationTimeMillis(invalidationTimeMillis.longValue());
      }
      return builder.build();
    }

    @Override
    public String toString() {
      StringBuilder builder = new StringBuilder();
      toString(builder);
      return builder.toString();
    }

    void toString(StringBuilder builder) {
      builder.append(isRegistered ? "REG " : "UNREG ").append(objectId.toString());
      if (payload != null) {
        builder.append(", payload=").append(payload.toStringUtf8());
      }
      if (highestVersion != null) {
        builder.append(", highestVersion=").append(highestVersion.longValue());
      }
      if (isBackground) {
        builder.append(", isBackground");
      }
      if (invalidationTimeMillis != null) {
        builder.append(", invalidationTime=").append(
            new Date(invalidationTimeMillis.longValue()).toString());
      }
    }
  }

  /** The tag used for logging in the listener state class. */
  private static final String TAG = "TEA2:ELS";

  /** Number of objects we're interested in tracking by default. */
  
  static final int NUM_INTERESTING_OBJECTS = 4;

  /** Object source for objects the client is initially tracking. */
  private static final int DEMO_SOURCE = 4;

  /** Prefix for object names the client is initially tracking. */
  private static final String OBJECT_ID_PREFIX = "Obj";

  /** State for all tracked objects. */
  private final Map<ObjectId, ObjectState> trackedObjects;

  /** Client id reported by {@code AndroidListener#ready} call. */
  private byte[] clientId;

  private ExampleListenerState(Map<ObjectId, ObjectState> trackedObjects,
      byte[] clientId) {
    this.trackedObjects = Preconditions.checkNotNull(trackedObjects);
    this.clientId = clientId;
  }

  public static ExampleListenerState deserialize(String data) {
    HashMap<ObjectId, ObjectState> trackedObjects = new HashMap<ObjectId, ObjectState>();
    byte[] clientId;
    ExampleListenerStateProto stateProto = tryParseStateProto(data);
    if (stateProto == null) {
      // By default, we're interested in objects with ids Obj1, Obj2, ...
      for (int i = 1; i <= NUM_INTERESTING_OBJECTS; ++i) {
        ObjectId objectId = ObjectId.newInstance(DEMO_SOURCE, (OBJECT_ID_PREFIX + i).getBytes());
        trackedObjects.put(objectId, new ObjectState(objectId, true));
      }
      clientId = null;
    } else {
      // Load interesting objects from state proto.
      for (ObjectStateProto objectStateProto : stateProto.getObjectStateList()) {
        ObjectState objectState = new ObjectState(objectStateProto);
        trackedObjects.put(objectState.objectId, objectState);
      }
      clientId = stateProto.hasClientId() ? stateProto.getClientId().toByteArray() : null;
    }
    return new ExampleListenerState(trackedObjects, clientId);
  }

  /** Returns proto serialized in data or null if it cannot be decoded. */
  private static ExampleListenerStateProto tryParseStateProto(String data) {
    if (data == null) {
      return null;
    }
    final byte[] bytes;
    try {
      bytes = Base64.decode(data, Base64.DEFAULT);
    } catch (IllegalArgumentException exception) {
      Log.e(TAG, String.format("Illegal base 64 encoding. data='%s', error='%s'", data,
          exception.getMessage()));
      return null;
    }
    try {
      ExampleListenerStateProto proto = ExampleListenerStateProto.parseFrom(bytes);
      return proto;
    } catch (InvalidProtocolBufferException exception) {
      Log.e(TAG, String.format("Error parsing state bytes. data='%s', error='%s'", data,
          exception.getMessage()));
      return null;
    }
  }

  /** Serializes example listener state to string. */
  public String serialize() {
    ExampleListenerStateProto.Builder builder = ExampleListenerStateProto.newBuilder();
    for (ObjectState objectState : trackedObjects.values()) {
      ObjectStateProto objectStateProto = objectState.serialize();
      builder.addObjectState(objectStateProto);
    }
    if (clientId != null) {
      builder.setClientId(ByteString.copyFrom(clientId));
    }
    ExampleListenerStateProto proto = builder.build();
    return Base64.encodeToString(proto.toByteArray(), Base64.DEFAULT);
  }

  Iterable<ObjectId> getInterestingObjects() {
    List<ObjectId> interestingObjects = new ArrayList<ObjectId>(trackedObjects.size());
    for (ObjectState objectState : trackedObjects.values()) {
      if (objectState.isRegistered) {
        interestingObjects.add(objectState.objectId);
      }
    }
    return interestingObjects;
  }

  byte[] getClientId() {
    return clientId;
  }

  /** Sets the client id passed to the example listener via the {@code ready()} call. */
  void setClientId(byte[] value) {
    clientId = value;
  }

  /**
   * Returns {@code true} if the state indicates a registration should exist for the given object.
   */
  boolean isInterestedInObject(ObjectId objectId) {
    ObjectState objectState = trackedObjects.get(objectId);
    return (objectState != null) && objectState.isRegistered;
  }

  /** Updates state for the given object to indicate it should be registered. */
  boolean addObjectOfInterest(ObjectId objectId) {
    ObjectState objectState = trackedObjects.get(objectId);
    if (objectState == null) {
      objectState = new ObjectState(objectId, true);
      trackedObjects.put(objectId, objectState);
      return true;
    }

    if (objectState.isRegistered) {
      return false;
    }
    objectState.isRegistered = true;
    return true;
  }

  /** Updates state for the given object to indicate it should not be registered. */
  boolean removeObjectOfInterest(ObjectId objectId) {
    ObjectState objectState = trackedObjects.get(objectId);
    if (objectState == null) {
      return false;
    }

    if (objectState.isRegistered) {
      objectState.isRegistered = false;
      return true;
    }
    return false;
  }

  /** Updates state for an object after an unknown-version invalidation is received. */
  void informUnknownVersionInvalidation(ObjectId objectId) {
    ObjectState objectState = getObjectStateForInvalidation(objectId);
    objectState.invalidationTimeMillis = System.currentTimeMillis();
    objectState.highestVersion = null;
    objectState.payload = null;
  }

  /** Updates state for an object after an invalidation is received. */
  void informInvalidation(ObjectId objectId, long version, byte[] payload,
      boolean isBackground) {
    ObjectState objectState = getObjectStateForInvalidation(objectId);
    if (objectState.highestVersion == null || objectState.highestVersion.longValue() < version) {
      objectState.highestVersion = version;
      objectState.payload = (payload == null) ? null : ByteString.copyFrom(payload);
      objectState.invalidationTimeMillis = System.currentTimeMillis();
      objectState.isBackground = isBackground;
    }
  }

  /**
   * Updates state when an invalidate all request is received (unknown version is marked for all
   * objects).
   */
  public void informInvalidateAll() {
    for (ObjectState objectState : trackedObjects.values()) {
      informUnknownVersionInvalidation(objectState.objectId);
    }
  }

  /** Returns existing object state for an object or updates state. */
  private ObjectState getObjectStateForInvalidation(ObjectId objectId) {
    ObjectState objectState = trackedObjects.get(objectId);
    if (objectState == null) {
      // Invalidation for unregistered object.
      objectState = new ObjectState(objectId, false);
      trackedObjects.put(objectId, objectState);
    }
    return objectState;
  }

  /** Returns an object given its serialized form. */
  static ObjectId deserializeObjectId(ObjectIdProto objectIdProto) {
    return ObjectId.newInstance(objectIdProto.getSource(), objectIdProto.getName().toByteArray());
  }

  /** Serializes the given object id. */
  static ObjectIdProto serializeObjectId(ObjectId objectId) {
    return ObjectIdProto.newBuilder()
        .setSource(objectId.getSource())
        .setName(ByteString.copyFrom(objectId.getName()))
        .build();
  }

  /** Clears all state for the example listener. */
  void clear() {
    trackedObjects.clear();
    clientId = null;
  }

  @Override
  public String toString() {
    StringBuilder builder = new StringBuilder();
    if (clientId != null) {
      builder.append("ready!\n");
    }
    for (ObjectState objectState : trackedObjects.values()) {
      objectState.toString(builder);
      builder.append("\n");
    }
    return builder.toString();
  }
}
