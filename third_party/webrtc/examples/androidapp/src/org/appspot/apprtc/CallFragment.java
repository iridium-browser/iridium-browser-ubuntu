/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import android.app.Activity;
import android.app.Fragment;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;

import org.webrtc.RendererCommon.ScalingType;

/**
 * Fragment for call control.
 */
public class CallFragment extends Fragment {
  private View controlView;
  private TextView contactView;
  private ImageButton disconnectButton;
  private ImageButton cameraSwitchButton;
  private ImageButton videoScalingButton;
  private OnCallEvents callEvents;
  private ScalingType scalingType;
  private boolean videoCallEnabled = true;

  /**
   * Call control interface for container activity.
   */
  public interface OnCallEvents {
    public void onCallHangUp();
    public void onCameraSwitch();
    public void onVideoScalingSwitch(ScalingType scalingType);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
      Bundle savedInstanceState) {
    controlView =
        inflater.inflate(R.layout.fragment_call, container, false);

    // Create UI controls.
    contactView =
        (TextView) controlView.findViewById(R.id.contact_name_call);
    disconnectButton =
        (ImageButton) controlView.findViewById(R.id.button_call_disconnect);
    cameraSwitchButton =
        (ImageButton) controlView.findViewById(R.id.button_call_switch_camera);
    videoScalingButton =
        (ImageButton) controlView.findViewById(R.id.button_call_scaling_mode);

    // Add buttons click events.
    disconnectButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        callEvents.onCallHangUp();
      }
    });

    cameraSwitchButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        callEvents.onCameraSwitch();
      }
    });

    videoScalingButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        if (scalingType == ScalingType.SCALE_ASPECT_FILL) {
          videoScalingButton.setBackgroundResource(
              R.drawable.ic_action_full_screen);
          scalingType = ScalingType.SCALE_ASPECT_FIT;
        } else {
          videoScalingButton.setBackgroundResource(
              R.drawable.ic_action_return_from_full_screen);
          scalingType = ScalingType.SCALE_ASPECT_FILL;
        }
        callEvents.onVideoScalingSwitch(scalingType);
      }
    });
    scalingType = ScalingType.SCALE_ASPECT_FILL;

    return controlView;
  }

  @Override
  public void onStart() {
    super.onStart();

    Bundle args = getArguments();
    if (args != null) {
      String contactName = args.getString(CallActivity.EXTRA_ROOMID);
      contactView.setText(contactName);
      videoCallEnabled = args.getBoolean(CallActivity.EXTRA_VIDEO_CALL, true);
    }
    if (!videoCallEnabled) {
      cameraSwitchButton.setVisibility(View.INVISIBLE);
    }
  }

  @Override
  public void onAttach(Activity activity) {
    super.onAttach(activity);
    callEvents = (OnCallEvents) activity;
  }

}
