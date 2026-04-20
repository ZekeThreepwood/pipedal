import React from 'react';
import { Theme } from '@mui/material/styles';
import { css } from '@emotion/react';
import { withStyles } from 'tss-react/mui';

import WithStyles from './WithStyles';
import PedalboardView from './PedalboardView';
import MidiBindingsDialog from './MidiBindingsDialog';
import LoadPluginDialog from './LoadPluginDialog';
import SnapshotDialog from './SnapshotDialog';
import PluginNameDialog from './PluginNameDialog';
import IconButtonEx from './IconButtonEx';

import InputIcon from '@mui/icons-material/Input';
import CameraAltOutlinedIcon from '@mui/icons-material/CameraAltOutlined';
import ArrowBackIosNewIcon from '@mui/icons-material/ArrowBackIosNew';
import ArrowForwardIosIcon from '@mui/icons-material/ArrowForwardIos';

import MidiIcon from "./svg/ic_midi.svg?react";
import OldDeleteIcon from "./svg/old_delete_outline_24dp.svg?react";

import { PiPedalModel } from './PiPedalModel';
import { PedalboardItem } from './Pedalboard';

const SCROLL_AMOUNT = 120;

const styles = (_theme: Theme) => {
    return {
        frame: css({
            position: "absolute",
            inset: 0,
            display: "flex",
            flexDirection: "column",
            overflow: "hidden",
        }),
        pedalboardViewport: css({
            position: "relative",
            flex: "1 1 auto",
            width: "100%",
            minHeight: 0,
            overflowX: "auto",
            overflowY: "hidden",
            display: "flex",
            alignItems: "center",
            justifyContent: "flex-start",
            paddingTop: 56,
            boxSizing: "border-box",
        }),
        pedalboardInner: css({
            position: "relative",
            flex: "0 0 auto",
            minWidth: "max-content",
            minHeight: 0,
            display: "flex",
            flexFlow: "row nowrap",
            alignItems: "center",
            justifyContent: "center",
            boxSizing: "border-box",
            margin: "0 auto",
        }),
        pedalboardEdgeSpacer: css({
            flex: "0 0 28px",
            width: 28,
            height: 1,
            pointerEvents: "none",
        }),
        pedalboardActionOverlayAnchor: css({
            position: "absolute",
            left: 0,
            right: 0,
            bottom: 0,
            zIndex: 3,
            display: "flex",
            justifyContent: "center",
            pointerEvents: "none",
        }),
        pedalboardActionOverlay: css({
            position: "absolute",
            bottom: 10,
            zIndex: 3,
            display: "flex",
            flexFlow: "row nowrap",
            alignItems: "center",
            justifyContent: "flex-end",
            gap: 4,
            paddingLeft: 8,
            paddingRight: 8,
            paddingTop: 6,
            paddingBottom: 6,
            borderRadius: 999,
            backdropFilter: "blur(6px)",
            WebkitBackdropFilter: "blur(6px)",
            background: "rgba(20,20,20,0.30)",
            boxSizing: "border-box",
            maxWidth: "calc(100% - 24px)",
            pointerEvents: "auto",
        }),
        controlArea: css({
            flex: "0 0 auto",
            width: "100%",
            overflow: "hidden",
        }),
        separator: css({
            flex: "0 0 auto",
            height: "1px",
            width: "100%",
            opacity: 0.15,
        }),
    };
};

interface MainPageSmallScreenState {
    canScrollLeft: boolean;
    canScrollRight: boolean;
}

export interface MainPageSmallScreenProps extends WithStyles<typeof styles> {
    model: PiPedalModel;
    pedalboardItem: PedalboardItem | null;
    pluginUri: string;
    pluginTitle: string;
    bypassVisible: boolean;
    bypassChecked: boolean;
    missing: boolean;
    canShowModUi: boolean;

    selectedPedal: number;
    showModUi: boolean;

    showMidiBindingsDialog: boolean;
    loadDialogOpen: boolean;
    snapshotDialogOpen: boolean;
    displayNameDialogOpen: boolean;

    enableStructureEditing: boolean;
    canInsert: boolean;
    canAppend: boolean;
    canLoad: boolean;
    canDelete: boolean;
    instanceId: number;

    onSelectionChanged: (selectedId: number) => void;
    onPedalDoubleClick: (selectedId: number) => void;
    onBypassChange: (event: any) => void;

    onToggleModUi: (instanceId: number, showModGui: boolean) => void;

    onCloseMidi: () => void;
    onLoadOk: (selectedUri: string) => void;
    onLoadCancel: () => void;
    onSnapshotOk: () => void;

    onOpenLoad: () => void;
    onOpenMidi: (instanceId: number) => void;
    onOpenSnapshot: () => void;
    onDeletePedal: (instanceId: number) => void;

    onInsertPedal: (instanceId: number) => void;
    onAppendPedal: (instanceId: number) => void;
    onInsertSplit: (instanceId: number) => void;
    onAppendSplit: (instanceId: number) => void;
    onAddAfter: (instanceId: number) => void;
    onSplitAfter: (instanceId: number) => void;
    onMergeAfter: (parentSplitId: number) => void;

    onCloseDisplayName: () => void;
    onApplyDisplayName: (newName: string, color: string) => void;

    getSelectedUri: () => string;
    excludePluginUris: string[];
}

const ICON_STYLE = { height: 24, width: 24, fill: "white", opacity: 0.6 };

const MainPageSmallScreen = withStyles(
    class MainPageSmallScreen extends React.Component<MainPageSmallScreenProps, MainPageSmallScreenState> {
        private viewportRef: React.RefObject<HTMLDivElement | null>;
        private scrollCheckTimer: number | null = null;

        constructor(props: MainPageSmallScreenProps) {
            super(props);
            this.state = {
                canScrollLeft: false,
                canScrollRight: false,
            };
            this.viewportRef = React.createRef();
            this.handleScroll = this.handleScroll.bind(this);
        }

        componentDidMount() {
            const el = this.viewportRef.current;
            if (el) {
                el.addEventListener('scroll', this.handleScroll);
            }
            // delay to let pedalboard content fully render
            this.scrollCheckTimer = window.setTimeout(() => {
                this.updateScrollState();
            }, 200);
        }

        componentWillUnmount() {
            const el = this.viewportRef.current;
            if (el) {
                el.removeEventListener('scroll', this.handleScroll);
            }
            if (this.scrollCheckTimer !== null) {
                clearTimeout(this.scrollCheckTimer);
            }
        }

        componentDidUpdate(prevProps: MainPageSmallScreenProps) {
            // re-check scroll when pedalboard content changes
            if (prevProps.pluginUri !== this.props.pluginUri ||
                prevProps.selectedPedal !== this.props.selectedPedal) {
                if (this.scrollCheckTimer !== null) clearTimeout(this.scrollCheckTimer);
                this.scrollCheckTimer = window.setTimeout(() => {
                    this.updateScrollState();
                }, 200);
            }
        }

        private updateScrollState() {
            const el = this.viewportRef.current;
            if (!el) return;
            const canScrollLeft = el.scrollLeft > 0;
            const canScrollRight = el.scrollLeft < el.scrollWidth - el.clientWidth - 1;
            this.setState({ canScrollLeft, canScrollRight });
        }

        private handleScroll() {
            this.updateScrollState();
        }

        private scrollLeft() {
            const el = this.viewportRef.current;
            if (el) {
                el.scrollBy({ left: -SCROLL_AMOUNT, behavior: 'smooth' });
            }
        }

        private scrollRight() {
            const el = this.viewportRef.current;
            if (el) {
                el.scrollBy({ left: SCROLL_AMOUNT, behavior: 'smooth' });
            }
        }

        render() {
            const classes = withStyles.getClasses(this.props);

            const {
                pedalboardItem,
                pluginUri,
                selectedPedal,
                showMidiBindingsDialog,
                loadDialogOpen,
                snapshotDialogOpen,
                displayNameDialogOpen,
                enableStructureEditing,
                canLoad,
                canDelete,
                instanceId,
                onSelectionChanged,
                onPedalDoubleClick,
                onCloseMidi,
                onLoadOk,
                onLoadCancel,
                onSnapshotOk,
                onOpenLoad,
                onOpenMidi,
                onOpenSnapshot,
                onDeletePedal,
                onAddAfter,
                onSplitAfter,
                onMergeAfter,
                onCloseDisplayName,
                onApplyDisplayName,
                getSelectedUri,
            } = this.props;

            const { canScrollLeft, canScrollRight } = this.state;

            return (
                <div className={classes.frame}>
                    <div
                        className={classes.pedalboardViewport}
                        ref={this.viewportRef}
                    >
                        <div className={classes.pedalboardInner}>
                            <div className={classes.pedalboardEdgeSpacer} />
                            <PedalboardView
                                key={pluginUri}
                                selectedId={selectedPedal}
                                enableStructureEditing={true}
                                onSelectionChanged={onSelectionChanged}
                                onDoubleClick={onPedalDoubleClick}
                                hasTinyToolBar={true}
                                onAddAfter={onAddAfter}
                                onSplitAfter={onSplitAfter}
                                onMergeAfter={onMergeAfter}
                            />
                            <div className={classes.pedalboardEdgeSpacer} />
                        </div>
                    </div>

                    {enableStructureEditing && (
                        <div className={classes.pedalboardActionOverlayAnchor}>
                            <div className={classes.pedalboardActionOverlay}>

                                {/* Scroll left — only when content exists to the left */}
                                {canScrollLeft && (
                                    <div style={{ flex: "0 0 auto" }}>
                                        <IconButtonEx
                                            tooltip="Scroll left"
                                            onClick={() => { this.scrollLeft(); }}
                                            size="large"
                                        >
                                            <ArrowBackIosNewIcon style={ICON_STYLE} />
                                        </IconButtonEx>
                                    </div>
                                )}

                                {/* Delete button */}
                                <div style={{ flex: "0 0 auto", display: canDelete ? "block" : "none" }}>
                                    <IconButtonEx
                                        tooltip="Delete pedal"
                                        onClick={() => { onDeletePedal(instanceId); }}
                                        size="large"
                                    >
                                        <OldDeleteIcon style={ICON_STYLE} />
                                    </IconButtonEx>
                                </div>

                                {/* Load button */}
                                <div style={{ flex: "0 0 auto" }}>
                                    <IconButtonEx
                                        tooltip="Load plugin"
                                        onClick={onOpenLoad}
                                        disabled={
                                            selectedPedal === -1 ||
                                            !canLoad ||
                                            !enableStructureEditing
                                        }
                                        size="large"
                                    >
                                        <InputIcon style={ICON_STYLE} />
                                    </IconButtonEx>
                                </div>

                                {/* MIDI button */}
                                <div style={{ flex: "0 0 auto" }}>
                                    <IconButtonEx
                                        tooltip="MIDI bindings"
                                        onClick={() => { onOpenMidi(instanceId); }}
                                        size="large"
                                    >
                                        <MidiIcon style={ICON_STYLE} />
                                    </IconButtonEx>
                                </div>

                                {/* Snapshot button */}
                                <div style={{ flex: "0 0 auto" }}>
                                    <IconButtonEx
                                        tooltip="Snapshots"
                                        onClick={onOpenSnapshot}
                                        size="large"
                                    >
                                        <CameraAltOutlinedIcon style={ICON_STYLE} />
                                    </IconButtonEx>
                                </div>

                                {/* Scroll right — only when content exists to the right */}
                                {canScrollRight && (
                                    <div style={{ flex: "0 0 auto" }}>
                                        <IconButtonEx
                                            tooltip="Scroll right"
                                            onClick={() => { this.scrollRight(); }}
                                            size="large"
                                        >
                                            <ArrowForwardIosIcon style={ICON_STYLE} />
                                        </IconButtonEx>
                                    </div>
                                )}

                            </div>
                        </div>
                    )}

                    <MidiBindingsDialog
                        open={showMidiBindingsDialog}
                        onClose={onCloseMidi}
                    />

                    {loadDialogOpen && (
                        <LoadPluginDialog
                            open={loadDialogOpen}
                            uri={getSelectedUri()}
                            onOk={onLoadOk}
                            onCancel={onLoadCancel}
                            excludeUris={this.props.excludePluginUris}
                        />
                    )}

                    {snapshotDialogOpen && (
                        <SnapshotDialog
                            open={snapshotDialogOpen}
                            onOk={onSnapshotOk}
                        />
                    )}

                    {displayNameDialogOpen && (
                        <PluginNameDialog
                            open={displayNameDialogOpen}
                            pedalboardItem={pedalboardItem}
                            allowEmpty={true}
                            onClose={onCloseDisplayName}
                            onApply={onApplyDisplayName}
                        />
                    )}
                </div>
            );
        }
    },
    styles
);

export default MainPageSmallScreen;
