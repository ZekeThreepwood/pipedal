import React from 'react';
import { Theme } from '@mui/material/styles';
import { css } from '@emotion/react';
import { withStyles } from 'tss-react/mui';
import WithStyles from './WithStyles';
import Select from '@mui/material/Select';
import MenuItem from '@mui/material/MenuItem';
import FormControl from '@mui/material/FormControl';
import InputLabel from '@mui/material/InputLabel';
import Typography from '@mui/material/Typography';
import { PiPedalModel, PiPedalModelFactory } from './PiPedalModel';
import { PedalboardItem } from './Pedalboard';
import JackConfiguration from './Jack';

const styles = (_theme: Theme) => ({
    frame: css({
        display: 'flex',
        flexDirection: 'column' as const,
        alignItems: 'center',
        justifyContent: 'center',
        padding: '24px 32px',
        gap: 20,
        height: '100%',
        boxSizing: 'border-box' as const,
    }),
    row: css({
        display: 'flex',
        flexDirection: 'column' as const,
        width: '100%',
        maxWidth: 360,
        gap: 8,
    }),
});

interface Props extends WithStyles<typeof styles> {
    instanceId: number;
    item: PedalboardItem;
    isInput: boolean;
    channelCount: number; // 1 = mono, 2 = stereo
}

interface State {
    jackConfig: JackConfiguration;
}

function friendlyPortName(raw: string, index: number): string {
    // "Focusrite 18i20 USB:capture_1" → "Channel 1 (Focusrite 18i20 USB)"
    // "system:capture_3" → "Channel 3"
    const colon = raw.indexOf(':');
    if (colon < 0) return raw;
    const device = raw.substring(0, colon).trim();
    const num = index + 1;
    const isSystem = device.toLowerCase() === 'system';
    return isSystem ? `Channel ${num}` : `Channel ${num}  (${device})`;
}

const InputOutputBoxControlView = withStyles(
    class extends React.Component<Props, State> {
        private model: PiPedalModel;

        constructor(props: Props) {
            super(props);
            this.model = PiPedalModelFactory.getInstance();
            this.state = { jackConfig: this.model.jackConfiguration.get() };
            this.onJackConfigChanged = this.onJackConfigChanged.bind(this);
        }

        componentDidMount() {
            this.model.jackConfiguration.addOnChangedHandler(this.onJackConfigChanged);
        }
        componentWillUnmount() {
            this.model.jackConfiguration.removeOnChangedHandler(this.onJackConfigChanged);
        }
        onJackConfigChanged(v: JackConfiguration) {
            this.setState({ jackConfig: v });
        }

        getCurrentChannel(): number {
            const key = this.props.isInput ? 'inputChannel' : 'outputChannel';
            return this.props.item.getControlValue(key);
        }

        handleChange(value: number) {
            const key = this.props.isInput ? 'inputChannel' : 'outputChannel';
            this.model.setPedalboardControl(this.props.instanceId, key, value);
        }

        render() {
            const classes = withStyles.getClasses(this.props);
            const { isInput, channelCount } = this.props;
            const { jackConfig } = this.state;

            const ports = isInput ? jackConfig.inputAudioPorts : jackConfig.outputAudioPorts;
            const label = isInput ? 'Hardware Input' : 'Hardware Output';
            const current = this.getCurrentChannel();

            // For stereo, last valid start = ports.length - 2
            const maxStart = Math.max(0, ports.length - channelCount);

            const options: { value: number; label: string }[] = [];
            for (let i = 0; i <= maxStart; ++i) {
                if (channelCount === 1) {
                    options.push({ value: i, label: friendlyPortName(ports[i] ?? '', i) });
                } else {
                    const a = friendlyPortName(ports[i] ?? '', i);
                    const b = friendlyPortName(ports[i + 1] ?? '', i + 1);
                    options.push({ value: i, label: `${a} + ${b}` });
                }
            }

            if (ports.length === 0) {
                return (
                    <div className={classes.frame}>
                        <Typography variant="body2" color="textSecondary">
                            No {isInput ? 'input' : 'output'} ports available.
                        </Typography>
                    </div>
                );
            }

            const safeValue = Math.min(current, maxStart);

            return (
                <div className={classes.frame}>
                    <div className={classes.row}>
                        <FormControl fullWidth variant="outlined" size="small">
                            <InputLabel>{label}</InputLabel>
                            <Select
                                value={safeValue}
                                label={label}
                                onChange={(e) => this.handleChange(Number(e.target.value))}
                            >
                                {options.map(opt => (
                                    <MenuItem key={opt.value} value={opt.value}>
                                        {opt.label}
                                    </MenuItem>
                                ))}
                            </Select>
                        </FormControl>
                    </div>
                </div>
            );
        }
    },
    styles
);

export default InputOutputBoxControlView;
