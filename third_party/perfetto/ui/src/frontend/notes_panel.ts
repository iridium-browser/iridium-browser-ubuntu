// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import * as m from 'mithril';

import {Actions} from '../common/actions';
import {timeToString} from '../common/time';

import {globals} from './globals';
import {gridlines} from './gridline_helper';
import {Panel, PanelSize} from './panel';
import {TRACK_SHELL_WIDTH} from './track_constants';

const FLAG_WIDTH = 10;

function toSummary(s: string) {
  const newlineIndex = s.indexOf('\n') > 0 ? s.indexOf('\n') : s.length;
  return s.slice(0, Math.min(newlineIndex, s.length, 16));
}

export class NotesPanel extends Panel {
  hoveredX: null|number = null;

  oncreate({dom}: m.CVnodeDOM) {
    dom.addEventListener('mousemove', (e: Event) => {
      this.hoveredX = (e as MouseEvent).layerX - TRACK_SHELL_WIDTH;
      globals.rafScheduler.scheduleRedraw();
    }, {passive: true});
    dom.addEventListener('mouseenter', (e: Event) => {
      this.hoveredX = (e as MouseEvent).layerX - TRACK_SHELL_WIDTH;
      globals.rafScheduler.scheduleRedraw();
    });
    dom.addEventListener('mouseout', () => {
      this.hoveredX = null;
      globals.rafScheduler.scheduleRedraw();
    }, {passive: true});
  }

  view() {
    return m('.notes-panel', {
      onclick: (e: MouseEvent) => {
        this.onClick(e.layerX - TRACK_SHELL_WIDTH, e.layerY);
      },
    });
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const timeScale = globals.frontendLocalState.timeScale;
    const range = globals.frontendLocalState.visibleWindowTime;
    let noteHovered = false;

    ctx.fillStyle = '#999';
    for (const xAndTime of gridlines(size.width, range, timeScale)) {
      ctx.fillRect(xAndTime[0], 0, 1, size.height);
    }

    ctx.textBaseline = 'bottom';
    ctx.font = '10px Google Sans';

    for (const note of Object.values(globals.state.notes)) {
      ctx.fillStyle = note.color;
      ctx.strokeStyle = note.color;
      const timestamp = note.timestamp;
      if (!timeScale.timeInBounds(timestamp)) continue;
      const x = timeScale.timeToPx(timestamp);

      const isHovered =
          this.hoveredX && x <= this.hoveredX && this.hoveredX < x + FLAG_WIDTH;
      const isSelected = globals.state.selectedNote === note.id;
      const left = Math.floor(x + TRACK_SHELL_WIDTH);
      const flagHeightPx = Math.ceil(size.height / 3);

      // Draw flag.
      ctx.fillRect(left, 1, 1, size.height - 1);
      if (!noteHovered && isHovered) {
        noteHovered = true;
        ctx.fillRect(left, 1, FLAG_WIDTH, flagHeightPx);
      } else if (isSelected) {
        ctx.fillRect(left, 1, FLAG_WIDTH, flagHeightPx);
      } else {
        ctx.fillStyle = 'white';
        ctx.fillRect(left, 1, FLAG_WIDTH, flagHeightPx);
        ctx.strokeRect(left + .5, 1.5, FLAG_WIDTH, flagHeightPx);
      }

      ctx.fillStyle = '#222';
      ctx.fillText(toSummary(note.text), left + 2, size.height - 1);
    }

    if (this.hoveredX !== null && !noteHovered) {
      ctx.fillStyle = 'black';
      const timestamp = timeScale.pxToTime(this.hoveredX);
      if (timeScale.timeInBounds(timestamp)) {
        const x = timeScale.timeToPx(timestamp);
        ctx.fillRect(Math.floor(x + TRACK_SHELL_WIDTH), 1, 1, size.height - 1);
      }
    }
  }

  private onClick(x: number, _: number) {
    const timeScale = globals.frontendLocalState.timeScale;
    const timestamp = timeScale.pxToTime(x);
    for (const note of Object.values(globals.state.notes)) {
      const noteX = timeScale.timeToPx(note.timestamp);
      if (noteX <= x && x < noteX + 10) {
        globals.dispatch(Actions.selectNote({id: note.id}));
        return;
      }
    }
    globals.dispatch(Actions.addNote({timestamp}));
  }
}

interface NotesEditorPanelAttrs {
  id: string;
}

export class NotesEditorPanel extends Panel<NotesEditorPanelAttrs> {
  view({attrs}: m.CVnode<NotesEditorPanelAttrs>) {
    const note = globals.state.notes[attrs.id];
    const startTime = note.timestamp - globals.state.traceTime.startSec;
    return m(
        '.notes-editor-panel',
        m('.notes-editor-panel-heading',
          `Annotation at time ${timeToString(startTime)} with color `,
          m('input[type=color]', {
            value: note.color,
            onchange: m.withAttr(
                'value',
                newColor => {
                  globals.dispatch(Actions.changeNoteColor({
                    id: attrs.id,
                    newColor,
                  }));
                }),
          }),
          m('button',
            {
              onclick: () =>
                  globals.dispatch(Actions.removeNote({id: attrs.id})),
            },
            'Remove'), ),
        m('textarea', {
          rows: 20,
          onkeydown: (e: Event) => {
            e.stopImmediatePropagation();
          },
          value: note.text,
          onchange: m.withAttr(
              'value',
              newText => {
                globals.dispatch(Actions.changeNoteText({
                  id: attrs.id,
                  newText,
                }));
              }),
        }), );
  }

  renderCanvas(_ctx: CanvasRenderingContext2D, _size: PanelSize) {}
}
