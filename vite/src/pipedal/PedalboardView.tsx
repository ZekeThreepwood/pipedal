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

import React, { ReactNode, Component, SyntheticEvent } from "react";
import { css } from "@emotion/react";
import { createStyles } from "./WithStyles";

import WithStyles, { withTheme } from "./WithStyles";
import { withStyles } from "tss-react/mui";
import IconButton from "@mui/material/IconButton";
import Menu from "@mui/material/Menu";
import MenuItem from "@mui/material/MenuItem";
import AddIcon from "@mui/icons-material/Add";
import CallMergeIcon from "@mui/icons-material/CallMerge";

import { Theme } from "@mui/material/styles";
import { PiPedalModel, PiPedalModelFactory } from "./PiPedalModel";
import { PluginType } from "./Lv2Plugin";
import ButtonBase from "@mui/material/ButtonBase";
import Typography from "@mui/material/Typography";
import PluginIcon, { getIconColor, SelectIconUri } from "./PluginIcon";
import { SelectHoverBackground } from "./SelectHoverBackground";
import SvgPathBuilder from "./SvgPathBuilder";
import Draggable from "./Draggable";
import Rect from "./Rect";
import { PiPedalStateError } from "./PiPedalError";
import Utility from "./Utility";
import { isDarkMode } from "./DarkMode";
import {
  Pedalboard,
  PedalboardItem,
  PedalboardSplitItem,
  SplitType,
} from "./Pedalboard";

// import MidiIcon from './svg/ic_midi.svg?react';
// import { midiChannelBindingControlFeatureEnabled } from './MidiChannelBinding';
// import CloseIcon from '@mui/icons-material/Close';

const START_CONTROL = Pedalboard.START_CONTROL;
const END_CONTROL = Pedalboard.END_CONTROL;

const START_PEDALBOARD_ITEM_URI = Pedalboard.START_PEDALBOARD_ITEM_URI;
const END_PEDALBOARD_ITEM_URI = Pedalboard.END_PEDALBOARD_ITEM_URI;

const ENABLED_CONNECTOR_COLOR = isDarkMode() ? "#CCC" : "#666";
const DISABLED_CONNECTOR_COLOR = isDarkMode() ? "#666" : "#CCC";

const CELL_WIDTH: number = 96;
const CELL_HEIGHT: number = 64;
const FRAME_SIZE: number = 36;

const I_SVG_STROKE_WIDTH = 3;
const I_SVG_STEREO_STROKE_WIDTH = 6;

const SVG_STROKE_WIDTH = I_SVG_STROKE_WIDTH.toString();
const SVG_STEREO_STROKE_WIDTH = I_SVG_STEREO_STROKE_WIDTH.toString();

const EMPTY_ICON_URL = "img/fx_empty.svg";
const ERROR_ICON_URL = "img/fx_error.svg";
const TERMINAL_ICON_URL = "img/fx_terminal.svg";

function CalculateConnection(numberOfInputs: number, numberOfOutputs: number) {
  if (numberOfInputs === 0) {
    return numberOfOutputs;
  }
  if (numberOfOutputs === 0) {
    return numberOfInputs;
  }
  let result = Math.min(numberOfInputs, numberOfOutputs);
  if (result > 2) result = 2;
  return result;
}

const pedalboardStyles = (theme: Theme) =>
  createStyles({
    scrollContainer: {},

    container: css({
      position: "relative",
      overflow: "visible",
    }),
    splitItem: css({
      position: "absolute",
      display: "flex",
      alignItems: "center",
      justifyContent: "center",

      width: CELL_WIDTH,
      height: CELL_HEIGHT,
    }),
    splitStart: css({
      position: "absolute",
      display: "flex",
      width: CELL_WIDTH,
      height: CELL_HEIGHT,
      left: 0,
      top: 0,
    }),
    splitEnd: css({
      position: "absolute",
      display: "flex",
      width: CELL_WIDTH,
      height: CELL_HEIGHT,
      right: 0,
      top: 0,
    }),
    buttonDraggable: css({
      display: "flex",
      alignItem: "center",
      justifyContent: "center",
      width: "100%",
      height: "100%",
    }),
    pedalItem: css({
      position: "absolute",
      width: CELL_WIDTH,
      height: CELL_HEIGHT,
      display: "flex",
      alignItems: "center",
      justifyContent: "center",
    }),
    midiConnectorDecoration: css({
      position: "absolute",
      left: -20,
      top: -6,
      fill: theme.palette.text.secondary,
    }),
    iconFrame: css({
      display: "flex",
      alignItems: "center",
      justifyContent: "center",
      position: "relative",

      background: theme.palette.background.default,
      marginLeft: (CELL_WIDTH - FRAME_SIZE) / 2,
      marginRight: (CELL_WIDTH - FRAME_SIZE) / 2,
      marginTop: (CELL_HEIGHT - FRAME_SIZE) / 2,
      marginBottom: (CELL_HEIGHT - FRAME_SIZE) / 2,
      width: FRAME_SIZE,
      height: FRAME_SIZE,
      border: isDarkMode() ? "1pt #555 solid" : "1pt #666 solid",
      borderRadius: 6,
    }),
    selectedIconFrame: css({
      display: "flex",
      alignItems: "center",
      justifyContent: "center",
      position: "relative",

      background: theme.palette.background.default,
      marginLeft: (CELL_WIDTH - FRAME_SIZE) / 2,
      marginRight: (CELL_WIDTH - FRAME_SIZE) / 2,
      marginTop: (CELL_HEIGHT - FRAME_SIZE) / 2,
      marginBottom: (CELL_HEIGHT - FRAME_SIZE) / 2,
      width: FRAME_SIZE,
      height: FRAME_SIZE,
      border: isDarkMode() ? "1pt #FFF solid" : "1pt #333 solid",
      borderRadius: 6,
    }),
    borderlessIconFrame: css({
      display: "flex",
      alignItems: "center",
      justifyContent: "center",

      background: "transparent",
      marginLeft: (CELL_WIDTH - FRAME_SIZE) / 2,
      marginRight: (CELL_WIDTH - FRAME_SIZE) / 2,
      marginTop: (CELL_HEIGHT - FRAME_SIZE) / 2,
      marginBottom: (CELL_HEIGHT - FRAME_SIZE) / 2,
      width: FRAME_SIZE,
      height: FRAME_SIZE,
      border: "0pt #666 solid",
      borderRadius: 6,
    }),

    pedalIcon: css({
      width: 24,
      height: 24,
      opacity: 0.8,
    }),
    connector: css({
      position: "absolute",
      width: CELL_WIDTH,
      height: CELL_HEIGHT,
    }),
    stroke: css({
      position: "absolute",
      background: "#888",
    }),
    stereoStrokeOuter: css({
      position: "absolute",
      background: "#888",
    }),
    stereoStrokeInner: css({
      position: "absolute",
      background: "#888",
    }),
  });

export type OnSelectHandler = (selectedPedal: number) => void;

interface PedalboardProps extends WithStyles<typeof pedalboardStyles> {
  theme: Theme;
  selectedId?: number;
  onSelectionChanged?: OnSelectHandler;
  onDoubleClick?: OnSelectHandler;
  hasTinyToolBar: boolean;
  enableStructureEditing: boolean;
  onAddAfter?: (instanceId: number) => void;
  onSplitAfter?: (instanceId: number) => void;
  onMergeAfter?: (parentSplitId: number) => void;
  onAddParallelChain?: () => void;
}
interface LayoutSize {
  width: number;
  height: number;
}

type PedalboardState = {
  pedalboard?: Pedalboard;
  splitMenuAnchor: HTMLElement | null;
  splitMenuInstanceId: number;
  showBlockButtons: boolean;
};

const EMPTY_PEDALS: PedalLayout[] = [];

function makeChain(
  model: PiPedalModel,
  uiItems?: PedalboardItem[],
  insideSplit: boolean = false,
  parentSplitId: number = -1,
): PedalLayout[] {
  let result: PedalLayout[] = [];
  if (uiItems) {
    for (let i = 0; i < uiItems.length; ++i) {
      let item = uiItems[i];
      let layout = new PedalLayout(model, item, insideSplit, parentSplitId);
      result.push(layout);
    }
  }
  return result;
}

class PedalLayout {
  uri: string = "";
  name: string = "";
  pluginType: PluginType = PluginType.Plugin;
  iconUrl: string = "";
  iconColor: string = "";
  isInsideSplit: boolean = false;
  parentSplitId: number = -1;
  hasMergeOutput: boolean = false; // set during doLayout2_ when split has a next sibling

  bounds: Rect = new Rect();

  numberOfInputs: number = 2;
  numberOfOutputs: number = 2;
  originalInputs: number = 2;
  originalOutputs: number = 2;

  pedalItem?: PedalboardItem;

  // Split Layout only.
  topChildren: PedalLayout[] = EMPTY_PEDALS;
  bottomChildren: PedalLayout[] = EMPTY_PEDALS;
  topConnectorY: number = 0;
  bottomConnectorY: number = 0;

  static Start(): PedalLayout {
    let t: PedalLayout = new PedalLayout();
    t.uri = START_PEDALBOARD_ITEM_URI;
    t.pluginType = PluginType.Terminal;
    t.iconUrl = TERMINAL_ICON_URL;
    t.numberOfInputs = 0;
    t.numberOfOutputs = 2;
    return t;
  }
  static End(): PedalLayout {
    let t: PedalLayout = new PedalLayout();
    t.pluginType = PluginType.Terminal;
    t.uri = END_PEDALBOARD_ITEM_URI;
    t.iconUrl = TERMINAL_ICON_URL;
    t.numberOfInputs = 2;
    t.numberOfOutputs = 0;
    return t;
  }
  constructor(model?: PiPedalModel, pedalItem?: PedalboardItem, insideSplit: boolean = false, parentSplitId: number = -1) {
    this.isInsideSplit = insideSplit;
    this.parentSplitId = parentSplitId;
    if (model === undefined && pedalItem === undefined) {
      return;
    }
    if (model === undefined || pedalItem === undefined) {
      throw new Error("Invalid arguments.");
    }
    this.pedalItem = pedalItem;
    this.uri = pedalItem.uri;
    if (pedalItem.isSplit()) {
      let splitter = pedalItem as PedalboardSplitItem;

      this.pluginType = PluginType.UtilityPlugin;
      this.topChildren = makeChain(model, splitter.topChain, true, splitter.instanceId);
      this.bottomChildren = makeChain(model, splitter.bottomChain, true, splitter.instanceId);

      this.numberOfInputs = 2;
      this.numberOfOutputs = 2;
    } else if (pedalItem.isEmpty()) {
      this.pluginType = PluginType.None;
      this.iconUrl = EMPTY_ICON_URL;
      this.numberOfInputs = 2;
      this.numberOfOutputs = 2;
    } else if (pedalItem.uri === START_PEDALBOARD_ITEM_URI) {
      this.pluginType = PluginType.UtilityPlugin;
      this.iconUrl = TERMINAL_ICON_URL;
      this.numberOfInputs = 0;
      this.numberOfOutputs =
        PiPedalModelFactory.getInstance().jackSettings.get().inputAudioPorts.length;
      if (this.numberOfOutputs === 0) {
        this.numberOfOutputs = 1;
      }
    } else if (pedalItem.uri === END_PEDALBOARD_ITEM_URI) {
      this.pluginType = PluginType.UtilityPlugin;
      this.iconUrl = TERMINAL_ICON_URL;
      this.numberOfInputs =
        PiPedalModelFactory.getInstance().jackSettings.get().outputAudioPorts.length;
      if (this.numberOfInputs === 0) {
        this.numberOfInputs = 1;
      }
      this.numberOfOutputs = 0;
    } else {
      let uiPlugin = model.getUiPlugin(pedalItem.uri);
      if (uiPlugin != null) {
        let pluginType = uiPlugin.plugin_type;
        this.pluginType = pluginType;
        if (this.uri === "http://two-play.com/plugins/toob-nam") {
          pluginType = PluginType.NamPlugin;
        }
        this.iconUrl = SelectIconUri(pluginType);
        this.iconColor = pedalItem.iconColor;
        this.name = uiPlugin.label;
        if (pedalItem.title !== "") {
          this.name = pedalItem.title;
        }
        this.numberOfInputs = Math.min(uiPlugin.audio_inputs, 2);
        this.numberOfOutputs = Math.min(uiPlugin.audio_outputs, 2);
      } else {
        // default to empty plugin.
        this.pluginType = PluginType.ErrorPlugin;
        this.name = pedalItem.pluginName ?? "#error";
        this.iconUrl = ERROR_ICON_URL;
        this.numberOfInputs = 2;
        this.numberOfOutputs = 2;
      }
    }
    this.originalInputs = this.numberOfInputs;
    this.originalOutputs = this.numberOfOutputs;
  }
  isEmpty(): boolean {
    return !this.pedalItem || this.pedalItem.isEmpty();
  }
  isSplitter(): boolean {
    return this.pedalItem !== undefined && this.pedalItem.isSplit();
  }
  isStart() {
    return this.uri === START_PEDALBOARD_ITEM_URI;
  }
  isEnd() {
    return this.uri === END_PEDALBOARD_ITEM_URI;
  }
  isInputBox() {
    return this.pluginType === PluginType.InputBox;
  }
  isOutputBox() {
    return this.pluginType === PluginType.OutputBox;
  }
}

function* chainIterator(
  layoutChain: PedalLayout[],
): Generator<PedalLayout, void, undefined> {
  for (let i = 0; i < layoutChain.length; ++i) {
    let item = layoutChain[i];
    yield item;
    if (item.isSplitter()) {
      let g = chainIterator(item.topChildren);
      while (true) {
        let v = g.next();
        if (v.done) {
          break;
        }
        yield v.value;
      }
      g = chainIterator(item.bottomChildren);
      while (true) {
        let v = g.next();
        if (v.done) {
          break;
        }
        yield v.value;
      }
    }
  }
  return;
}

class LayoutParams {
  nextId: number = 1;
  cx: number = 0;
  cy: number = 0;
}

const PedalboardView = withTheme(
  withStyles(
    class extends Component<PedalboardProps, PedalboardState> {
      model: PiPedalModel;

      frameRef: React.RefObject<HTMLDivElement | null>;
      scrollRef: React.RefObject<HTMLDivElement | null>;
      private bgColor: string;

      constructor(props: PedalboardProps) {
        super(props);
        this.model = PiPedalModelFactory.getInstance();

        this.bgColor = props.theme.palette.background.default;

        if (!props.selectedId) props.selectedId = -1;
        this.state = {
          pedalboard: this.model.pedalboard.get(),
          splitMenuAnchor: null,
          splitMenuInstanceId: -1,
          showBlockButtons: false,
        };
        this.onPedalboardChanged = this.onPedalboardChanged.bind(this);
        this.frameRef = React.createRef();
        this.scrollRef = React.createRef();
        this.handleTouchStart = this.handleTouchStart.bind(this);
        this.openSplitMenu = this.openSplitMenu.bind(this);
        this.closeSplitMenu = this.closeSplitMenu.bind(this);
      }

      handleTouchStart(e: any) {
        // just has to exist to allow Draggable to receive
        // touchyMove. :-/
      }

      openSplitMenu(e: React.MouseEvent<HTMLElement>, instanceId: number) {
        e.stopPropagation();
        this.setState({ splitMenuAnchor: e.currentTarget, splitMenuInstanceId: instanceId });
      }
      closeSplitMenu() {
        this.setState({ splitMenuAnchor: null, splitMenuInstanceId: -1 });
      }

      onDragEnd(instanceId: number, clientX: number, clientY: number) {
        if (!this.props.enableStructureEditing) {
          return;
        }
        if (!this.currentLayout) return;

        if (!this.frameRef.current) return;

        let currentLayout: PedalLayout[] = this.currentLayout;
        let frameElement = this.frameRef.current;

        let rc = frameElement.getBoundingClientRect();
        clientX -= rc.left;
        clientY -= rc.top;

        let it = chainIterator(currentLayout);

        while (true) {
          let v = it.next();
          if (v.done) break;
          let item = v.value;

          if (item.isSplitter() && item.pedalItem) {
            if (item.bounds.contains(clientX, clientY)) {
              if (clientX < item.bounds.x + CELL_WIDTH / 2) {
                this.model.movePedalboardItemBefore(
                  instanceId,
                  item.pedalItem.instanceId,
                );
                this.setSelection(instanceId);
                return;
              } else if (clientX > item.bounds.right - CELL_WIDTH / 2) {
                this.model.movePedalboardItemAfter(
                  instanceId,
                  item.pedalItem.instanceId,
                );
                this.setSelection(instanceId);
                return;
              }
            }
            let yMid = (item.bounds.y + item.bounds.bottom) / 2;
            if (
              clientX >= item.bounds.x &&
              clientY < yMid &&
              clientY >= item.topChildren[0].bounds.y
            ) {
              if (clientX < item.topChildren[0].bounds.x) {
                let topPedalItem = item.topChildren[0].pedalItem;
                if (topPedalItem) {
                  this.model.movePedalboardItemBefore(
                    instanceId,
                    topPedalItem.instanceId,
                  );
                  this.setSelection(instanceId);
                  return;
                }
              }
              let lastTop = item.topChildren[item.topChildren.length - 1];
              if (
                clientX >= lastTop.bounds.right &&
                clientX < item.bounds.right - CELL_WIDTH / 2
              ) {
                if (lastTop.pedalItem) {
                  this.model.movePedalboardItemAfter(
                    instanceId,
                    lastTop.pedalItem.instanceId,
                  );
                  this.setSelection(instanceId);
                  return;
                }
              }
            }
            if (
              clientX >= item.bounds.x &&
              clientY > yMid &&
              clientY < item.bottomChildren[0].bounds.bottom
            ) {
              if (clientX < item.bottomChildren[0].bounds.x) {
                let bottomPedalItem = item.bottomChildren[0].pedalItem;
                if (bottomPedalItem) {
                  this.model.movePedalboardItemBefore(
                    instanceId,
                    bottomPedalItem.instanceId,
                  );
                  this.setSelection(instanceId);
                  return;
                }
              }
              let lastBottom =
                item.bottomChildren[item.bottomChildren.length - 1];
              if (
                clientX >= lastBottom.bounds.right &&
                clientX < item.bounds.right - CELL_WIDTH / 2
              ) {
                if (lastBottom.pedalItem) {
                  this.model.movePedalboardItemAfter(
                    instanceId,
                    lastBottom.pedalItem.instanceId,
                  );
                  this.setSelection(instanceId);
                  return;
                }
              }
            }
          } else if (item.bounds.contains(clientX, clientY)) {
            if (item.isStart()) {
              this.model.movePedalboardItemToStart(instanceId);
              this.setSelection(instanceId);
              return;
            } else if (item.isEnd()) {
              this.model.movePedalboardItemToEnd(instanceId);
              this.setSelection(instanceId);
              return;
            } else {
              if (item.pedalItem) {
                let margin = (CELL_WIDTH - FRAME_SIZE) / 2;
                if (clientX < item.bounds.x + margin) {
                  this.model.movePedalboardItemBefore(
                    instanceId,
                    item.pedalItem.instanceId,
                  );
                } else if (clientX > item.bounds.right - margin) {
                  this.model.movePedalboardItemAfter(
                    instanceId,
                    item.pedalItem.instanceId,
                  );
                } else {
                  this.model.movePedalboardItem(
                    instanceId,
                    item.pedalItem.instanceId,
                  );
                }
                this.setSelection(instanceId);
                return;
              }
            }
          }
        }
        // delete the plugin.
        let newId = this.model.setPedalboardItemEmpty(instanceId);
        this.setSelection(newId);
      }

      onPedalboardChanged(value?: Pedalboard) {
        this.setState({
          pedalboard: value,
        });
      }

      componentDidMount() {
        this.scrollRef.current!.addEventListener(
          "touchstart",
          this.handleTouchStart,
          { passive: false },
        );
        this.model.pedalboard.addOnChangedHandler(this.onPedalboardChanged);
      }
      componentWillUnmount() {
        this.scrollRef.current!.removeEventListener(
          "touchstart",
          this.handleTouchStart,
        );
        this.model.pedalboard.removeOnChangedHandler(this.onPedalboardChanged);
      }

      offsetLayout_(layoutItems: PedalLayout[], offset: number): void {
        for (let i = 0; i < layoutItems.length; ++i) {
          let layoutItem = layoutItems[i];
          layoutItem.bounds.y += offset;
          if (layoutItem.isSplitter()) {
            layoutItem.topConnectorY += offset;
            layoutItem.bottomConnectorY += offset;
            this.offsetLayout_(layoutItem.topChildren, offset);
            this.offsetLayout_(layoutItem.bottomChildren, offset);
          }
        }
      }

      getSplitterIcon(layoutItem: PedalLayout): PluginType {
        if (layoutItem.pedalItem === undefined) {
          throw new Error("Invalid splitter");
        }
        let split = layoutItem.pedalItem as PedalboardSplitItem;
        if (split.getSplitType() === SplitType.Ab) {
          if (split.isASelected()) {
            return PluginType.SplitA;
          } else {
            return PluginType.SplitB;
          }
        } else if (split.getSplitType() === SplitType.Mix) {
          return PluginType.SplitMix; //"img/fx_dial.svg";
        } else {
          return PluginType.SplitLR; //"img/fx_lr.svg";
        }
      }

      doLayout2_(lp: LayoutParams, layoutItems: PedalLayout[]): Rect {
        let bounds = new Rect();
        for (let i = 0; i < layoutItems.length; ++i) {
          let layoutItem = layoutItems[i];
          if (layoutItem.isSplitter()) {
            let x0 = lp.cx;
            let y0 = lp.cy;

            layoutItem.bounds.x = x0;
            layoutItem.bounds.y = y0;
            layoutItem.bounds.height = CELL_HEIGHT;
            layoutItem.bounds.width = CELL_WIDTH;

            lp.cx += CELL_WIDTH;

            let topBounds = this.doLayout2_(lp, layoutItem.topChildren);
            if (topBounds.isEmpty()) {
              topBounds.x = lp.cx;
              topBounds.width = 0;
              topBounds.y = y0 - CELL_HEIGHT / 2;
              topBounds.height = CELL_HEIGHT;
            }

            let dyTop =
              lp.cy + CELL_HEIGHT / 2 - (topBounds.y + topBounds.height);

            this.offsetLayout_(layoutItem.topChildren, dyTop);
            topBounds.offset(0, dyTop);
            bounds.accumulate(topBounds);

            let topCx = lp.cx;
            lp.cx = x0;
            lp.cx += CELL_WIDTH;

            let bottomBounds = this.doLayout2_(lp, layoutItem.bottomChildren);
            if (bottomBounds.isEmpty()) {
              bottomBounds.x = lp.cx;
              bottomBounds.width = 0;
              bottomBounds.y = lp.cy;
              bottomBounds.height = CELL_HEIGHT;
            }

            let dyBottom = lp.cy + CELL_HEIGHT / 2 - bottomBounds.y;
            this.offsetLayout_(layoutItem.bottomChildren, dyBottom);
            bottomBounds.offset(0, dyBottom);
            bounds.accumulate(bottomBounds);

            layoutItem.hasMergeOutput = i < layoutItems.length - 1;
            lp.cx = Math.max(lp.cx, topCx) + (layoutItem.hasMergeOutput ? CELL_WIDTH : 0);
            lp.cy = y0;

            layoutItem.bounds.width = lp.cx - layoutItem.bounds.x;
            bounds.accumulate(layoutItem.bounds);

            if (layoutItem.topChildren.length === 0) {
              layoutItem.topConnectorY = bounds.y + CELL_HEIGHT / 2;
            } else {
              layoutItem.topConnectorY =
                layoutItem.topChildren[0].bounds.y + CELL_HEIGHT / 2;
            }
            if (layoutItem.bottomChildren.length === 0) {
              layoutItem.bottomConnectorY =
                bounds.y + bounds.height - CELL_HEIGHT / 2;
            } else {
              layoutItem.bottomConnectorY =
                layoutItem.bottomChildren[0].bounds.y + CELL_HEIGHT / 2;
            }
          } else {
            layoutItem.bounds.x = lp.cx;
            layoutItem.bounds.y = lp.cy;
            lp.cx += CELL_WIDTH;
            layoutItem.bounds.width = CELL_WIDTH;
            layoutItem.bounds.height = CELL_HEIGHT;
            bounds.accumulate(layoutItem.bounds);
          }
        }
        return bounds;
      }
      doLayout(layoutItems: PedalLayout[]): LayoutSize {
        const TWO_ROW_HEIGHT = 142 - 14;

        if (layoutItems.length === 0) {
          // if the current pedalboard is empty, reserve display space anyway.
          return { width: 1, height: TWO_ROW_HEIGHT };
        }

        let lp = new LayoutParams();

        let bounds = this.doLayout2_(lp, layoutItems);
        // shift everything down so there are no negative y coordinates.

        if (bounds.height < TWO_ROW_HEIGHT) {
          let extra = Math.floor(
            (TWO_ROW_HEIGHT - Math.ceil(bounds.height)) / 2,
          );
          this.offsetLayout_(layoutItems, Math.floor(-bounds.y + extra / 2));
          bounds.height += extra;
        } else {
          this.offsetLayout_(layoutItems, -bounds.y);
        }

        bounds.height += 14; // for labels that aren't accounted for.
        return { width: bounds.width, height: bounds.height };
      }

      private lastClickInstanceId: number = -1;
      private lastClickTime: number = 0;

      onItemClick(e: SyntheticEvent, instanceId?: number): void {
        if (!instanceId && instanceId !== 0) return;

        const now = Date.now();
        if (
          this.lastClickInstanceId === instanceId &&
          now - this.lastClickTime < 400
        ) {
          this.lastClickTime = 0;
          this.lastClickInstanceId = -1;
          if (this.props.onDoubleClick && this.props.enableStructureEditing) {
            this.props.onDoubleClick(instanceId);
          }
        } else {
          this.lastClickTime = now;
          this.lastClickInstanceId = instanceId;
          this.setState({ showBlockButtons: true });
          if (instanceId !== this.props.selectedId) {
            this.setSelection(instanceId);
          }
        }
      }
      setSelection(instanceId: number) {
        if (this.props.onSelectionChanged) {
          this.props.onSelectionChanged(instanceId);
        }
      }

      onItemDoubleClick(event: SyntheticEvent, instanceId?: number): void {
        event.preventDefault();
        event.stopPropagation();

        if (
          this.props.onDoubleClick &&
          instanceId &&
          this.props.enableStructureEditing
        ) {
          this.props.onDoubleClick(instanceId);
        }
      }

      onItemLongClick(event: SyntheticEvent, instanceId?: number): void {
        if (!instanceId) {
          return;
        }
        event.preventDefault();
        event.stopPropagation();

        if (!this.props.enableStructureEditing) {
          this.setSelection(instanceId);
          return;
        }
        if (!Utility.needsZoomedControls()) {
          if (
            this.props.onDoubleClick &&
            this.props.enableStructureEditing &&
            instanceId
          ) {
            this.props.onDoubleClick(instanceId);
          }
        }
      }

      strokeConnector(
        output: ReactNode[],
        channels: number,
        enabled: Boolean,
        svgPath: string,
      ) {
        let color = enabled
          ? ENABLED_CONNECTOR_COLOR
          : DISABLED_CONNECTOR_COLOR;

        if (channels === 2) {
          output.push(
            <path
              key={this.renderKey++}
              d={svgPath}
              stroke={color}
              strokeWidth={SVG_STEREO_STROKE_WIDTH}
            />,
          );
          output.push(
            <path
              key={this.renderKey++}
              d={svgPath}
              stroke={this.bgColor}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        } else if (channels === 1) {
          output.push(
            <path
              key={this.renderKey++}
              d={svgPath}
              stroke={color}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        }
      }

      renderConnector(
        output: ReactNode[],
        item: PedalLayout,
        enabled: boolean,
      ): void {
        // const classes = withStyles.getClasses(this.props);
        let x_ = item.bounds.x + CELL_WIDTH / 2;
        let y_ = item.bounds.y + CELL_HEIGHT / 2;
        let numberOfOutputs = item.numberOfOutputs;
        let color = enabled
          ? ENABLED_CONNECTOR_COLOR
          : DISABLED_CONNECTOR_COLOR;
        let stereoCenterColor = this.bgColor;

        if (item.originalInputs === 0) {
          // break the input paths.
          let rx = item.bounds.x + CELL_WIDTH / 2 - FRAME_SIZE / 2 - 4;
          let ry = y_ - 4;

          output.push(
            <rect
              key={this.renderKey++}
              x={rx}
              y={ry}
              width={4}
              height={8}
              fill={this.props.theme.palette.background.paper}
            />,
          );
        }
        let svgPath = new SvgPathBuilder()
          .moveTo(x_, y_)
          .lineTo(x_ + CELL_WIDTH, y_)
          .toString();

        if (numberOfOutputs === 2) {
          output.push(
            <path
              key={this.renderKey++}
              d={svgPath}
              stroke={color}
              strokeWidth={SVG_STEREO_STROKE_WIDTH}
            />,
          );
          output.push(
            <path
              key={this.renderKey++}
              d={svgPath}
              stroke={stereoCenterColor}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        } else if (numberOfOutputs === 1) {
          output.push(
            <path
              key={this.renderKey++}
              d={svgPath}
              stroke={color}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        } else {
          output.push(
            <path
              key={this.renderKey++}
              d={svgPath}
              stroke={DISABLED_CONNECTOR_COLOR}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        }
      }
      renderSplitConnectors(
        output: ReactNode[],
        item: PedalLayout,
        enabled: boolean,
        shortSplitOutput: boolean,
      ): void {
        //const classes = withStyles.getClasses(this.props);
        let x_ = item.bounds.x + CELL_WIDTH / 2;
        let y_ = item.bounds.y + CELL_HEIGHT / 2;
        let yTop = item.topConnectorY;
        let yBottom = item.bottomConnectorY;
        //let isStereo = item.stereoOutput;
        let split = item.pedalItem as PedalboardSplitItem;

        let topEnabled = enabled && split.isASelected();
        let bottomEnabled = enabled && split.isBSelected();
        let topColor = topEnabled
          ? ENABLED_CONNECTOR_COLOR
          : DISABLED_CONNECTOR_COLOR;
        let bottomColor = bottomEnabled
          ? ENABLED_CONNECTOR_COLOR
          : DISABLED_CONNECTOR_COLOR;

        let topStartPath = new SvgPathBuilder()
          .moveTo(x_, y_)
          .lineTo(x_, yTop)
          .lineTo(x_ + CELL_WIDTH, yTop)
          .toString();
        let bottomStartPath = new SvgPathBuilder()
          .moveTo(x_, y_)
          .lineTo(x_, yBottom)
          .lineTo(x_ + CELL_WIDTH, yBottom)
          .toString();

        if (
          item.numberOfInputs === 2 &&
          item.topChildren[0].numberOfInputs === 2
        ) {
          output.push(
            <path
              key={this.renderKey++}
              d={topStartPath}
              stroke={topColor}
              strokeWidth={SVG_STEREO_STROKE_WIDTH}
            />,
          );
          output.push(
            <path
              key={this.renderKey++}
              d={topStartPath}
              stroke={this.bgColor}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        } else if (
          item.numberOfInputs !== 0 &&
          item.topChildren[0].numberOfInputs !== 0
        ) {
          output.push(
            <path
              key={this.renderKey++}
              d={topStartPath}
              stroke={topColor}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        }

        if (
          item.numberOfInputs === 2 &&
          item.bottomChildren[0].numberOfInputs === 2
        ) {
          output.push(
            <path
              key={this.renderKey++}
              d={bottomStartPath}
              stroke={bottomColor}
              strokeWidth={SVG_STEREO_STROKE_WIDTH}
            />,
          );
          output.push(
            <path
              key={this.renderKey++}
              d={bottomStartPath}
              stroke={this.bgColor}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        } else if (
          item.numberOfInputs !== 0 &&
          item.bottomChildren[0].numberOfInputs !== 0
        ) {
          output.push(
            <path
              key={this.renderKey++}
              d={bottomStartPath}
              stroke={bottomColor}
              strokeWidth={SVG_STROKE_WIDTH}
            />,
          );
        }

        // If no next item in main chain, this is a pure Y-fork — no merge wire
        if (!item.hasMergeOutput) return;

        // Draw merge wires from both branch ends back to the merge output cell
        let lastTop = item.topChildren[item.topChildren.length - 1];
        let lastBottom = item.bottomChildren[item.bottomChildren.length - 1];

        let xTopLast = lastTop.bounds.right - CELL_WIDTH / 2;
        let xBottomLast = lastBottom.bounds.right - CELL_WIDTH / 2;
        let xConverge = item.bounds.right - CELL_WIDTH;
        let xMerge = item.bounds.right - CELL_WIDTH / 2;

        let color = enabled ? ENABLED_CONNECTOR_COLOR : DISABLED_CONNECTOR_COLOR;
        let strokeW = item.numberOfOutputs === 2 ? SVG_STEREO_STROKE_WIDTH : SVG_STROKE_WIDTH;

        let topMergePath = new SvgPathBuilder()
          .moveTo(xTopLast, yTop)
          .lineTo(xConverge, yTop)
          .lineTo(xConverge, y_)
          .lineTo(xMerge, y_)
          .toString();
        let bottomMergePath = new SvgPathBuilder()
          .moveTo(xBottomLast, yBottom)
          .lineTo(xConverge, yBottom)
          .lineTo(xConverge, y_)
          .toString();

        // Wire from merge point to next block in main chain
        let outputPath = new SvgPathBuilder()
          .moveTo(xMerge, y_)
          .lineTo(xMerge + CELL_WIDTH, y_)
          .toString();

        output.push(<path key={this.renderKey++} d={bottomMergePath} stroke={color} strokeWidth={strokeW} />);
        output.push(<path key={this.renderKey++} d={topMergePath} stroke={color} strokeWidth={strokeW} />);
        output.push(<path key={this.renderKey++} d={outputPath} stroke={color} strokeWidth={strokeW} />);
        if (item.numberOfOutputs === 2) {
          output.push(<path key={this.renderKey++} d={bottomMergePath} stroke={this.bgColor} strokeWidth={SVG_STROKE_WIDTH} />);
          output.push(<path key={this.renderKey++} d={topMergePath} stroke={this.bgColor} strokeWidth={SVG_STROKE_WIDTH} />);
          output.push(<path key={this.renderKey++} d={outputPath} stroke={this.bgColor} strokeWidth={SVG_STROKE_WIDTH} />);
        }
      }
      getScrollContainer() {
        let el: HTMLElement | undefined | null = this.scrollRef.current;
        // actually not here anymore. :-/ It has a reactive definition in MainPage.tsx now.
        while (el) {
          if (el.id === "pedalboardScroll") {
            return el as HTMLDivElement;
          }
          el = el.parentElement;
        }
        throw new PiPedalStateError("scroll container not found.");
      }

      pedalButton(
        instanceId: number,
        iconType: PluginType,
        iconColor: string,
        draggable: boolean,
        enabled: boolean,
        hasBorder: boolean = true,
        pluginNotFound: boolean,
        hasMidiConnector: boolean,
      ): ReactNode {
        const classes = withStyles.getClasses(this.props);
        let frameStyle = classes.iconFrame;
        if (!hasBorder) {
          frameStyle = classes.borderlessIconFrame;
        } else {
          if (instanceId === this.props.selectedId) {
            frameStyle = classes.selectedIconFrame;
          }
        }

        return (
          <div
            className={frameStyle}
            onContextMenu={(e) => {
              e.preventDefault();
            }}
          >
            {/* {!enabled && (
                                <div className={classes.midiConnectorDecoration} >
                                    <CloseIcon style={{ width: 16, height: 16, opacity: 0.6, fill: this.props.theme.palette.text.secondary }} />
                                </div>

                            )} */}

            <ButtonBase
              style={{ width: "100%", height: "100%" }}
              onClick={(e) => {
                this.onItemClick(e, instanceId);
              }}
              onContextMenu={(e: SyntheticEvent) => {
                this.onItemLongClick(e, instanceId);
              }}
            >
              <SelectHoverBackground
                selected={instanceId === this.props.selectedId}
                showHover={true}
                borderRadius={6}
                clipChildren={true}
              >
                <Draggable
                  draggable={draggable && this.props.enableStructureEditing}
                  getScrollContainer={() => this.getScrollContainer()}
                  onDragEnd={(x, y) => {
                    this.onDragEnd(instanceId, x, y);
                  }}
                  style={{ opacity: enabled ? 0.99 : 0.3 }}
                >
                  <div id="childIcon" style={{ position: "relative" }}>
                    <PluginIcon
                      pluginType={iconType}
                      size={24}
                      color={getIconColor(iconColor)}
                      pluginMissing={pluginNotFound}
                    />
                  </div>
                </Draggable>
              </SelectHoverBackground>
            </ButtonBase>
          </div>
        );
      }
      renderConnectors(
        output: ReactNode[],
        layoutChain: PedalLayout[],
        enabled: boolean,
        shortSplitOutput: boolean,
      ): void {
        let length = layoutChain.length - 1;
        if (
          layoutChain.length > 0 &&
          layoutChain[layoutChain.length - 1].isSplitter()
        ) {
          ++length;
        }
        for (let i = 0; i < length; ++i) {
          let item = layoutChain[i];
          if (item.isSplitter()) {
            let splitter = item.pedalItem as PedalboardSplitItem;
            this.renderSplitConnectors(
              output,
              item,
              enabled,
              i === length - 1 && shortSplitOutput,
            );
            this.renderConnectors(
              output,
              item.topChildren,
              enabled && splitter.isASelected(),
              false,
            );
            this.renderConnectors(
              output,
              item.bottomChildren,
              enabled && splitter.isBSelected(),
              false,
            );
          } else if (item.uri !== END_PEDALBOARD_ITEM_URI && !item.isOutputBox()) {
            this.renderConnector(output, item, enabled);
          }
        }
      }
      renderConnectorFrame(
        layoutChain: PedalLayout[],
        layoutSize: LayoutSize,
      ): ReactNode {
        let outputs: ReactNode[] = [];
        this.renderConnectors(outputs, layoutChain, true, false);
        return (
          <div
            key="connectors"
            style={{
              width: layoutSize.width,
              height: layoutSize.height,
              overflow: "hidden",
            }}
          >
            <svg
              width={layoutSize.width}
              height={layoutSize.height}
              xmlns="http://www.w3.org/2000/svg"
              viewBox={"0 0 " + layoutSize.width + " " + layoutSize.height}
            >
              <g fill="none">{outputs}</g>
            </svg>
          </div>
        );
      }

      renderChain(
        layoutChain: PedalLayout[],
        layoutSize: LayoutSize,
      ): ReactNode {
        const classes = withStyles.getClasses(this.props);

        let result: ReactNode[] = [];

        result.push(this.renderConnectorFrame(layoutChain, layoutSize));

        let it = chainIterator(layoutChain);
        while (true) {
          let v = it.next();
          if (v.done) break;
          let item = v.value;
          switch (item.uri) {
            case START_PEDALBOARD_ITEM_URI:
              result.push(
                <div
                  key={this.renderKey++}
                  className={classes.splitItem}
                  style={{
                    left: item.bounds.x,
                    top: item.bounds.y,
                    width: item.bounds.width,
                  }}
                >
                  <div className={classes.splitStart}>
                    {this.pedalButton(
                      START_CONTROL,
                      item.pluginType,
                      item.iconColor,
                      false,
                      true,
                      false,
                      false,
                      false,
                    )}
                  </div>
                </div>,
              );
              break;
            case END_PEDALBOARD_ITEM_URI:
              result.push(
                <div
                  key={this.renderKey++}
                  className={classes.splitItem}
                  style={{
                    left: item.bounds.x,
                    top: item.bounds.y,
                    width: item.bounds.width,
                  }}
                >
                  <div className={classes.splitStart}>
                    {this.pedalButton(
                      END_CONTROL,
                      item.pluginType,
                      "",
                      false,
                      true,
                      false,
                      false,
                      false,
                    )}
                  </div>
                </div>,
              );
              break;
            default:
              if (item.isOutputBox()) {
                result.push(
                  <div
                    key={this.renderKey++}
                    className={classes.pedalItem}
                    style={{ left: item.bounds.x, top: item.bounds.y }}
                  >
                    {this.pedalButton(
                      item.pedalItem?.instanceId ?? -1,
                      item.pluginType,
                      item.iconColor,
                      false,
                      true,
                      false,
                      false,
                      false,
                    )}
                  </div>,
                );
              } else if (item.isSplitter()) {
                // Splitter is invisible — Y-wire connectors are drawn by renderSplitConnectors
              } else {
                result.push(
                  <div
                    key={this.renderKey++}
                    style={{
                      display: "flex",
                      justifyContent: "flex-start",
                      alignItems: "flex-start",
                      position: "absolute",
                      left: item.bounds.x,
                      width: CELL_WIDTH,
                      top: item.bounds.bottom - 12,
                      paddingLeft: 2,
                      paddingRight: 2,
                    }}
                  >
                    <Typography
                      variant="caption"
                      display="block"
                      noWrap={true}
                      style={{
                        width: CELL_WIDTH - 4,
                        textAlign: "center",
                        flex: "0 1 auto",
                        opacity:
                          (item.pedalItem?.isEnabled ?? true) ? 1.0 : 0.4,
                      }}
                    >
                      {item.name}
                    </Typography>
                  </div>,
                );
                let uiPlugin = this.model.getUiPlugin(
                  item.pedalItem?.uri ?? "",
                );
                let pluginMissing = uiPlugin === null;
                let pluginType = item.pluginType;
                if (
                  uiPlugin &&
                  uiPlugin.uri === "http://two-play.com/plugins/toob-nam"
                ) {
                  pluginType = PluginType.NamPlugin;
                }

                const blockInstanceId = item.pedalItem?.instanceId ?? -1;
                const isSelected = blockInstanceId === this.props.selectedId;
                result.push(
                  <div
                    key={this.renderKey++}
                    className={classes.pedalItem}
                    style={{ left: item.bounds.x, top: item.bounds.y }}
                  >
                    {this.pedalButton(
                      blockInstanceId,
                      pluginType,
                      item.pedalItem?.iconColor ?? "",
                      !item.isEmpty() && !item.isInputBox(),
                      item.pedalItem?.isEnabled ?? false,
                      !item.isInputBox(),
                      pluginMissing,
                      uiPlugin
                        ? uiPlugin.has_midi_input !== 0 ||
                            uiPlugin.has_midi_output !== 0
                        : false,
                    )}
                    {isSelected && this.props.enableStructureEditing && this.state.showBlockButtons && (
                      <>
                        <div style={{ position: "absolute", right: -12, top: CELL_HEIGHT / 2 - 12, zIndex: 20 }}>
                          <IconButton
                            size="small"
                            style={{ width: 24, height: 24, background: this.props.theme.palette.background.paper, border: "1px solid #888" }}
                            onClick={(e) => { e.stopPropagation(); this.setState({ showBlockButtons: false }); this.props.onAddAfter?.(blockInstanceId); }}
                          >
                            <AddIcon style={{ width: 16, height: 16 }} />
                          </IconButton>
                        </div>
                        <div style={{ position: "absolute", bottom: -12, left: CELL_WIDTH / 2 - 12, zIndex: 20 }}>
                          <IconButton
                            size="small"
                            style={{ width: 24, height: 24, background: this.props.theme.palette.background.paper, border: "1px solid #888" }}
                            onClick={(e) => {
                              this.setState({ showBlockButtons: false });
                              if (item.isInsideSplit) {
                                this.props.onMergeAfter?.(item.parentSplitId);
                              } else {
                                this.openSplitMenu(e, blockInstanceId);
                              }
                            }}
                          >
                            {item.isInsideSplit
                              ? <CallMergeIcon style={{ width: 16, height: 16 }} />
                              : <AddIcon style={{ width: 16, height: 16 }} />
                            }
                          </IconButton>
                        </div>
                      </>
                    )}
                  </div>,
                );
              }
              break;
          }
        }
        return result;
      }

      canInputStero(item: PedalLayout): boolean {
        if (item.pedalItem) {
          let plugin = this.model.getUiPlugin(item.pedalItem.uri);
          if (plugin) {
            return plugin.audio_inputs === 2;
          }
        }
        return true;
      }
      getNumberOfInputs(item: PedalLayout): number {
        if (item.pedalItem) {
          let plugin = this.model.getUiPlugin(item.pedalItem.uri);
          if (plugin) {
            return plugin.audio_inputs;
          }
        }
        return 1;
      }
      getNumberOfOutputs(item: PedalLayout): number {
        if (item.pedalItem) {
          let plugin = this.model.getUiPlugin(item.pedalItem.uri);
          if (plugin) {
            return plugin.audio_outputs;
          }
        }
        return 1;
      }

      markStereoOutputs(
        layoutChain: PedalLayout[],
        numberOfInputs: number,
        numberOfOutputs: number,
      ) {
        // analyze forward flow.
        this.markStereoForward(layoutChain, numberOfInputs);
        // mark items that feed a mono effect as mono.
        this.markStereoBackward(layoutChain, numberOfOutputs);
      }

      markStereoBackward(
        layoutChain: PedalLayout[],
        numberOfOutputs: number,
      ): number {
        for (let i = layoutChain.length - 1; i >= 0; --i) {
          let item = layoutChain[i];
          if (item.isSplitter()) {
            item.numberOfOutputs = CalculateConnection(
              item.numberOfOutputs,
              numberOfOutputs,
            );

            this.markStereoBackward(item.topChildren, numberOfOutputs);
            this.markStereoBackward(item.bottomChildren, numberOfOutputs);
            let topInputs = item.topChildren[0].numberOfInputs;
            let bottomInputs = item.bottomChildren[0].numberOfInputs;

            let splitItem = item.pedalItem as PedalboardSplitItem;
            if (splitItem.getSplitType() !== SplitType.Lr) {
              item.numberOfInputs = CalculateConnection(
                item.numberOfInputs,
                Math.max(topInputs, bottomInputs),
              );
            }
          } else if (item.isEnd()) {
          } else if (item.isStart()) {
            item.numberOfOutputs = CalculateConnection(
              item.numberOfOutputs,
              numberOfOutputs,
            );
            return item.numberOfOutputs;
          } else if (item.isOutputBox()) {
            // OutputBox is a sink — nothing propagates backward past it
          } else if (item.isInputBox()) {
            item.numberOfOutputs = CalculateConnection(item.numberOfOutputs, numberOfOutputs);
            return item.numberOfOutputs;
          } else if (item.isEmpty()) {
            if (numberOfOutputs === 0) {
              item.numberOfOutputs = 0;
              item.numberOfInputs = CalculateConnection(item.numberOfInputs, 2);
            } else {
              item.numberOfOutputs = CalculateConnection(
                item.numberOfOutputs,
                numberOfOutputs,
              );
              item.numberOfInputs = CalculateConnection(
                item.numberOfInputs,
                numberOfOutputs,
              );
            }
          } else {
            item.numberOfOutputs = CalculateConnection(
              item.numberOfOutputs,
              numberOfOutputs,
            );
          }
          numberOfOutputs = item.numberOfInputs;
        }
        return numberOfOutputs;
      }
      markStereoForward(
        layoutChain: PedalLayout[],
        numberOfInputs: number,
      ): number {
        if (layoutChain.length === 0) {
          return numberOfInputs;
        }
        for (let i = 0; i < layoutChain.length; ++i) {
          let item = layoutChain[i];
          if (item.isSplitter()) {
            let splitter = item.pedalItem as PedalboardSplitItem;
            item.numberOfInputs = numberOfInputs;

            let chainInputs = numberOfInputs;
            if (splitter.getSplitType() === SplitType.Lr) {
              chainInputs = CalculateConnection(numberOfInputs, 1);
            }
            let topOutputs = this.markStereoForward(
              item.topChildren,
              chainInputs,
            );
            let bottomOutputs = this.markStereoForward(
              item.bottomChildren,
              chainInputs,
            );

            if (splitter.getSplitType() === SplitType.Ab) {
              if (splitter.isASelected()) {
                item.numberOfOutputs = topOutputs;
              } else {
                item.numberOfOutputs = bottomOutputs;
              }
            } else {
              item.numberOfOutputs =
                topOutputs >= 1 || bottomOutputs >= 1 ? 2 : 1;
            }
          } else if (item.isStart()) {
            item.numberOfOutputs = Math.min(
              PiPedalModelFactory.getInstance().jackSettings.get()
                .inputAudioPorts.length,
              2,
            );
          } else if (item.isEnd()) {
            item.numberOfInputs = CalculateConnection(
              Math.min(
                PiPedalModelFactory.getInstance().jackSettings.get()
                  .outputAudioPorts.length,
                2,
              ),
              numberOfInputs,
            );
            return item.numberOfInputs;
          } else if (item.isInputBox()) {
            // Fixed hardware source — channel count declared by the plugin, not from upstream
            item.numberOfOutputs = Math.min(item.originalOutputs, 2);
          } else if (item.isOutputBox()) {
            // Hardware sink — accept whatever channels flow in
            item.numberOfInputs = CalculateConnection(item.originalInputs, numberOfInputs);
            return item.numberOfInputs;
          } else if (item.isEmpty()) {
            item.numberOfInputs = numberOfInputs;
            if (numberOfInputs === 0) {
              item.numberOfOutputs = 2;
            } else {
              item.numberOfOutputs = item.numberOfInputs;
            }
          } else {
            if (
              item.numberOfInputs === 0
            ) // zero-input plugins merge their output with the input.
            {
              item.numberOfInputs = numberOfInputs;
              item.numberOfOutputs = Math.max(
                item.numberOfOutputs,
                numberOfInputs,
              );
            } else {
              item.numberOfInputs = CalculateConnection(
                numberOfInputs,
                this.getNumberOfInputs(item),
              );
              item.numberOfOutputs = this.getNumberOfOutputs(item);
            }
          }
          numberOfInputs = item.numberOfOutputs;
        }
        return numberOfInputs;
      }

      currentLayout?: PedalLayout[];
      private renderKey: number = 0;

      private readonly PARALLEL_SEPARATOR = 24;

      render() {
        const classes = withStyles.getClasses(this.props);
        this.renderKey = 0;

        const pedalboard = this.state.pedalboard;
        const chainItemArrays: (PedalboardItem[] | undefined)[] = [
          pedalboard?.items,
          ...(pedalboard?.parallelChains ?? []),
        ];

        const allLayoutChains = chainItemArrays.map(items => makeChain(this.model, items));
        for (const lc of allLayoutChains) {
          if (lc.length !== 0) this.markStereoOutputs(lc, 2, 2);
        }
        const allLayoutSizes = allLayoutChains.map(lc => this.doLayout(lc));

        const totalWidth = allLayoutSizes.reduce((max, s) => Math.max(max, s.width), 1);

        const chainYOffsets: number[] = [0];
        let totalHeight = allLayoutSizes[0]?.height ?? 1;
        for (let i = 1; i < allLayoutChains.length; ++i) {
          chainYOffsets.push(totalHeight + this.PARALLEL_SEPARATOR);
          totalHeight += this.PARALLEL_SEPARATOR + allLayoutSizes[i].height;
        }

        this.currentLayout = allLayoutChains[0]; // DnD only on main chain

        return (
          <>
            <div className={classes.scrollContainer} ref={this.scrollRef}>
              <div
                className={classes.container}
                ref={this.frameRef}
                style={{ width: totalWidth, height: totalHeight }}
              >
                {allLayoutChains.map((lc, i) => (
                  <React.Fragment key={i}>
                    {i > 0 && (
                      <div style={{
                        position: 'absolute',
                        left: 0,
                        right: 0,
                        top: chainYOffsets[i] - this.PARALLEL_SEPARATOR / 2,
                        height: 1,
                        background: 'rgba(128,128,128,0.3)',
                      }} />
                    )}
                    <div style={{
                      position: 'absolute',
                      left: 0,
                      top: chainYOffsets[i],
                      width: totalWidth,
                      height: allLayoutSizes[i].height,
                    }}>
                      {this.renderChain(lc, allLayoutSizes[i])}
                    </div>
                  </React.Fragment>
                ))}
              </div>
            </div>
            <Menu
              anchorEl={this.state.splitMenuAnchor}
              open={Boolean(this.state.splitMenuAnchor)}
              onClose={this.closeSplitMenu}
            >
              <MenuItem onClick={() => { const id = this.state.splitMenuInstanceId; this.closeSplitMenu(); this.props.onSplitAfter?.(id); }}>
                Split chain
              </MenuItem>
              <MenuItem onClick={() => { this.closeSplitMenu(); this.props.onAddParallelChain?.(); }}>
                Parallel branch
              </MenuItem>
            </Menu>
          </>
        );
      }
    },
    pedalboardStyles,
  ),
);

export default PedalboardView;
