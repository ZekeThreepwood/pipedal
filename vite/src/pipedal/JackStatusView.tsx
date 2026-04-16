// Copyright (c) 2022 Robin Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import React from "react";
import { PiPedalModel, PiPedalModelFactory, State } from "./PiPedalModel";
import JackHostStatus from "./JackHostStatus";

interface JackStatusViewProps {}

interface JackStatusViewState {
  jackStatus?: JackHostStatus;
}

export default class JackStatusView extends React.Component<
  JackStatusViewProps,
  JackStatusViewState
> {
  model: PiPedalModel;

  constructor(props: JackStatusViewProps) {
    super(props);
    this.state = {
      jackStatus: undefined,
    };
    this.model = PiPedalModelFactory.getInstance();
    this.tick = this.tick.bind(this);
  }

  tick() {
    if (this.model.state.get() === State.Ready) {
      this.model
        .getJackStatus()
        .then((jackStatus) => {
          this.setState({ jackStatus: jackStatus });
        })
        .catch((error) => {
          /* ignore*/
        });
    }
  }

  timerHandle?: number;
  componentDidMount() {
    this.timerHandle = setInterval(this.tick, 1000);
  }
  componentWillUnmount() {
    if (this.timerHandle) {
      clearTimeout(this.timerHandle);
      this.timerHandle = undefined;
    }
  }

  render() {
    const status = this.state.jackStatus;

    let dotColor = '#888';
    if (status) {
        if (status.restarting) {
            dotColor = '#fa0';
        } else if (!status.active) {
            dotColor = '#17f213';
        } else {
            dotColor = '#4c4';
        }
    }

    return (
        <div style={{
            position: "fixed",
            top: 6,
            right: 8,
            height: 30,
            paddingRight: 6,
            paddingTop: 4,
            opacity: 0.7,
            whiteSpace: "nowrap",
            fontSize: 12,
            zIndex: 10,
            fontWeight: 900,
            display: "flex",
            alignItems: "center",
            gap: 6,
        }}>
            <div style={{
                width: 10,
                height: 10,
                borderRadius: "50%",
                background: dotColor,
                flexShrink: 0,
            }} />
            {status && status.active && JackHostStatus.getDisplayView("", status)}
            {status && !status.active && status.temperaturemC > -100000 && (
                <span style={{ fontSize: 12, color: status.temperaturemC > 75000 ? '#f44' : 'rgba(255,255,255,0.7)' }}>
                    {(status.temperaturemC / 1000).toFixed(1)}°C
                </span>
            )}
        </div>
    );
}
}
