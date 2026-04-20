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

import { PiPedalArgumentError } from './PiPedalError';
import MidiBinding from './MidiBinding';
import MidiChannelBinding from './MidiChannelBinding';


const SPLIT_PEDALBOARD_ITEM_URI = "uri://two-play/pipedal/pedalboard#Split";
const EMPTY_PEDALBOARD_ITEM_URI = "uri://two-play/pipedal/pedalboard#Empty";

interface Deserializable<T> {
    deserialize(input: any): T;
}

export class ControlValue implements Deserializable<ControlValue> {
    constructor(key?: string, value?: number)
    {
        this.key = key??"";
        this.value = value?? 0;
    }
    deserialize(input: any): ControlValue {
        this.key = input.key;
        this.value = input.value;
        return this;
    }
    static EmptyArray: ControlValue[] = [];

    static deserializeArray(input: any[]): ControlValue[] {
        let result: ControlValue[] = [];
        for (let i = 0; i < input.length; ++i) {
            result[i] = new ControlValue().deserialize(input[i]);
        }
        return result;
    }
    setValue(value: number) {
        this.value = value;
    }

    key: string;
    value: number;

}

export class PedalboardItem implements Deserializable<PedalboardItem> {
    deserializePedalboardItem(input: any): PedalboardItem {
        this.instanceId = input.instanceId ?? -1;
        this.title = input.title ?? "";
        this.uri = input.uri;
        this.pluginName = input.pluginName;
        this.isEnabled = input.isEnabled;
        this.midiBindings = MidiBinding.deserialize_array(input.midiBindings);
        if (input.midiChannelBinding)
        {
            this.midiChannelBinding = input.midiChannelBinding;
        } else {
            this.midiChannelBinding = null;
        }

        this.controlValues = ControlValue.deserializeArray(input.controlValues);
        this.vstState = input.vstState ?? "";
        this.stateUpdateCount = input.stateUpdateCount;
        this.lv2State = input.lv2State;
        this.lilvPresetUri = input.lilvPresetUri;
        this.pathProperties = input.pathProperties;
        this.useModUi = input.useModUi ?? false;
        this.iconColor = input.iconColor??"";
        this.sideChainInputId = input.sideChainInputId ?? -1;

        return this;
    }
    deserialize(input: any): PedalboardItem {
        return this.deserializePedalboardItem(input);
    }
    static deserializeArray(input: any): PedalboardItem[] {
        let result: PedalboardItem[] = [];
        for (let i = 0; i < input.length; ++i) {
            let inputItem: any = input[i];
            let uri: string = inputItem.uri as string;
            let outputItem: PedalboardItem;

            if (uri === SPLIT_PEDALBOARD_ITEM_URI) {
                outputItem = new PedalboardSplitItem().deserialize(inputItem);
            } else {
                outputItem = new PedalboardItem().deserialize(inputItem);
            }
            result[i] = outputItem;

        }
        return result;
    }

    isSyntheticItem(): boolean {
        return this.instanceId === Pedalboard.START_CONTROL
        || this.instanceId === Pedalboard.END_CONTROL;
    }
    getInstanceId() : number {
        if (this.instanceId === undefined)
        {
            throw new PiPedalArgumentError("Item does not have an id.");
        }
        return this.instanceId;
    }
    isEmpty(): boolean {
        return this.uri === EMPTY_PEDALBOARD_ITEM_URI;
    }
    isSplit(): boolean {
        return this.uri === SPLIT_PEDALBOARD_ITEM_URI;
    }
    isStart(): boolean {
        return this.uri === Pedalboard.START_PEDALBOARD_ITEM_URI;
    }
    isEnd(): boolean {
        return this.uri === Pedalboard.END_PEDALBOARD_ITEM_URI;
    }

    getControl(key: string): ControlValue {
        for (let i = 0; i < this.controlValues.length; ++i) {
            let v = this.controlValues[i];
            if (v.key === key) {
                return v;
            }
        }
        throw new PiPedalArgumentError("Invalid key.");

    }

    getControlValue(key: string): number {
        for (let i = 0; i < this.controlValues.length; ++i) {
            let v = this.controlValues[i];
            if (v.key === key) {
                return v.value;
            }
        }
        return 0;
    }
    setControlValue(key: string, value: number): boolean {
        for (let i = 0; i < this.controlValues.length; ++i) {
            let v = this.controlValues[i];
            if (v.key === key) {
                if (v.value === value) return false;
                v.setValue(value);
                return true;
            }
        }
        return false;
    }
    setMidiBinding(midiBinding: MidiBinding): boolean {
        if (this.midiBindings)
        {
            for (let i = 0; i < this.midiBindings.length; ++i)
            {
                let binding = this.midiBindings[i];
                if (midiBinding.symbol === binding.symbol)
                {
                    if (binding.equals(midiBinding))
                    {
                        return false;
                    }
                    this.midiBindings.splice(i,1,midiBinding);
                    return true;
                }
            }
            this.midiBindings.push(midiBinding);
            return true;
        }
        this.midiBindings = [ midiBinding];
        return true;
    }
    getMidiBinding(symbol: string): MidiBinding {
        if (this.midiBindings)
        {
            for (let i = 0; i < this.midiBindings.length; ++i)
            {
                let midiBinding = this.midiBindings[i];
                if (midiBinding.symbol === symbol)
                {
                    return midiBinding;
                }
            }
        }
        let result = new MidiBinding();
        result.symbol = symbol;
        return result;
    }


    static EmptyArray: PedalboardItem[] = [];

    instanceId: number = -1;
    title: string = "";
    isEnabled: boolean = false;
    uri: string = "";
    pluginName?: string;
    controlValues: ControlValue[] = ControlValue.EmptyArray;
    midiBindings: MidiBinding[] = [];
    midiChannelBinding: MidiChannelBinding | null = null;
    vstState: string = "";
    stateUpdateCount: number = 0;
    lv2State: [boolean,any] = [false,{}];
    lilvPresetUri: string = "";
    pathProperties: {[Name: string]: string} = {};
    useModUi: boolean = false; // true if this item should use the mod-ui.
    iconColor: string = "";
    sideChainInputId: number = -1; // -1 means no sidechain input.
};

export class SnapshotValue {
    deserialize(input: any): SnapshotValue {
        this.isEnabled = input.isEnabled;
        this.instanceId = input.instanceId;
        this.controlValues = ControlValue.deserializeArray(input.controlValues);
        this.lv2State = input.lv2State;
        this.pathProperties = input.pathProperties;
        return this;
    }
    static deserializeArray(input: any): SnapshotValue[] {
        let result: SnapshotValue[] = [];
        for (let i = 0; i < input.length; ++i) {
            let inputItem: any = input[i];
            let outputItem = new SnapshotValue().deserialize(inputItem);
            result[i] = outputItem;

        }
        return result;
    }
    static createFromPedalboardItem(item: PedalboardItem)
    {
        let result = new SnapshotValue();
        result.instanceId = item.instanceId;
        result.isEnabled = item.isEnabled;
        result.controlValues = ControlValue.deserializeArray(item.controlValues); 
        result.lv2State = item.lv2State; // we can do this, because lv2State is immutable.
        result.pathProperties = {...item.pathProperties}; // clone the dictionary.
        return result;
    }
    instanceId: number = -1;
    isEnabled: boolean = true;
    controlValues: ControlValue[] = ControlValue.EmptyArray;
    lv2State: [boolean,any] = [false,{}];
    pathProperties: {[Name: string]: string} = {};
}
export class Snapshot {
    deserialize(input: any): Snapshot {
        this.values = SnapshotValue.deserializeArray(input.values);
        this.isModified = input.isModified;
        this.name = input.name;
        this.color = input.color;
        return this;
    }
    static deserializeArray(input: any): (Snapshot| null)[] {
        let result: (Snapshot|null)[] = [];
        for (let i = 0; i < input.length; ++i) {
            let inputItem: any = input[i];
            let outputItem: (Snapshot|null) = null;
            if (inputItem !== null)
            {
                outputItem = new Snapshot().deserialize(inputItem);
            }
            result[i] = outputItem;
        }
        return result;
    }
    static readonly  MAX_SNAPSHOTS: number = 6;

    static cloneSnapshots(snapshots: (Snapshot|null)[]): (Snapshot|null)[]
    {
        let result: (Snapshot|null)[] = [];
        for (let i = 0; i < Snapshot.MAX_SNAPSHOTS; ++i)
        {
            if (i >= snapshots.length) 
            {
                result.push(null);
            } else {
                result.push(snapshots[i]);
            }
        }
        return result;
    }
    name: string = "";
    isModified: boolean = false;
    color: string = "";
    values: SnapshotValue[] = [];
};

export enum SplitType {
    Ab = 0,
    Mix = 1,
    Lr = 2
}

// ---------------------------------------------------------------------------
// US-10: RoutingGraph model — mirrors RoutingGraph.hpp / RoutingGraph.cpp.
// ---------------------------------------------------------------------------

export const INVALID_BOX_ID = 0;
export type BoxType = "Plugin" | "Merge" | "Ab" | "Output";

export class Box {
    id: number = INVALID_BOX_ID;
    type: BoxType = "Plugin";
    isEnabled: boolean = true;
    title: string = "";
    iconColor: string = "";
    pluginUri: string = "";
    pluginName: string = "";
    controlValues: ControlValue[] = [];
    outputChannelIndex: number = 0;
    mergeVolume: number = 0;
    mergePanL: number = 0;
    mergePanR: number = 0;
    abSelectedInput: number = 0;

    deserialize(input: any): Box {
        this.id                 = input.id                 ?? INVALID_BOX_ID;
        this.type               = input.type               ?? "Plugin";
        this.isEnabled          = input.isEnabled          ?? true;
        this.title              = input.title              ?? "";
        this.iconColor          = input.iconColor          ?? "";
        this.pluginUri          = input.pluginUri          ?? "";
        this.pluginName         = input.pluginName         ?? "";
        this.controlValues      = ControlValue.deserializeArray(input.controlValues ?? []);
        this.outputChannelIndex = input.outputChannelIndex ?? 0;
        this.mergeVolume        = input.mergeVolume        ?? 0;
        this.mergePanL          = input.mergePanL          ?? 0;
        this.mergePanR          = input.mergePanR          ?? 0;
        this.abSelectedInput    = input.abSelectedInput    ?? 0;
        return this;
    }

    static deserializeArray(input: any[]): Box[] {
        return (input ?? []).map((x: any) => new Box().deserialize(x));
    }

    isPlugin():  boolean { return this.type === "Plugin"; }
    isMerge():   boolean { return this.type === "Merge";  }
    isAb():      boolean { return this.type === "Ab";     }
    isOutput():  boolean { return this.type === "Output"; }
    isEmpty():   boolean { return this.pluginUri === EMPTY_PEDALBOARD_ITEM_URI; }

    getControlValue(key: string): number {
        const cv = this.controlValues.find(c => c.key === key);
        return cv ? cv.value : 0;
    }
    setControlValue(key: string, value: number): boolean {
        const cv = this.controlValues.find(c => c.key === key);
        if (cv) {
            if (cv.value === value) return false;
            cv.value = value;
            return true;
        }
        this.controlValues.push(new ControlValue(key, value));
        return true;
    }
}

export class Connection {
    from: number = INVALID_BOX_ID;
    to:   number = INVALID_BOX_ID;

    constructor(from?: number, to?: number) {
        this.from = from ?? INVALID_BOX_ID;
        this.to   = to   ?? INVALID_BOX_ID;
    }
    deserialize(input: any): Connection {
        this.from = input.from ?? INVALID_BOX_ID;
        this.to   = input.to   ?? INVALID_BOX_ID;
        return this;
    }
    static deserializeArray(input: any[]): Connection[] {
        return (input ?? []).map((x: any) => new Connection().deserialize(x));
    }
}

export class RoutingGraph {
    name:        string       = "";
    nextBoxId:   number       = 1;
    boxes:       Box[]        = [];
    connections: Connection[] = [];

    deserialize(input: any): RoutingGraph {
        this.name        = input.name        ?? "";
        this.nextBoxId   = input.nextBoxId   ?? 1;
        this.boxes       = Box.deserializeArray(input.boxes ?? []);
        this.connections = Connection.deserializeArray(input.connections ?? []);
        return this;
    }

    isEmpty(): boolean { return this.boxes.length === 0; }

    findBox(id: number): Box | null {
        return this.boxes.find(b => b.id === id) ?? null;
    }

    getUpstream(id: number): Box[] {
        return this.connections
            .filter(c => c.to === id)
            .map(c => this.findBox(c.from))
            .filter((b): b is Box => b !== null);
    }

    getDownstream(id: number): Box[] {
        return this.connections
            .filter(c => c.from === id)
            .map(c => this.findBox(c.to))
            .filter((b): b is Box => b !== null);
    }

    getOutputBoxes():  Box[] { return this.boxes.filter(b => b.isOutput()); }
    getPluginBoxes():  Box[] { return this.boxes.filter(b => b.isPlugin()); }
    getRootBoxes():    Box[] {
        const hasIncoming = new Set(this.connections.map(c => c.to));
        return this.boxes.filter(b => !hasIncoming.has(b.id));
    }

    // Kahn's topological sort — returns boxes in signal-flow order.
    // Returns [] if there is a cycle.
    topologicalSort(): Box[] {
        const inDegree = new Map<number, number>();
        for (const b of this.boxes) inDegree.set(b.id, 0);
        for (const c of this.connections) {
            inDegree.set(c.to, (inDegree.get(c.to) ?? 0) + 1);
        }
        const queue: number[] = [];
        for (const [id, deg] of inDegree) {
            if (deg === 0) queue.push(id);
        }
        const sorted: Box[] = [];
        let head = 0;
        while (head < queue.length) {
            const current = queue[head++];
            const box = this.findBox(current);
            if (box) sorted.push(box);
            for (const c of this.connections) {
                if (c.from === current) {
                    const nd = (inDegree.get(c.to) ?? 1) - 1;
                    inDegree.set(c.to, nd);
                    if (nd === 0) queue.push(c.to);
                }
            }
        }
        return sorted.length === this.boxes.length ? sorted : [];
    }

    // Build a RoutingGraph from the legacy items[] tree — mirrors
    // Pedalboard.cpp::RebuildRoutingGraph. Used as fallback for old preset
    // files that have no "routingGraph" key.
    static fromItems(items: PedalboardItem[], name: string): RoutingGraph {
        const g = new RoutingGraph();
        g.name = name;

        const outputBox = new Box();
        outputBox.id    = g.nextBoxId++;
        outputBox.type  = "Output";
        outputBox.title = "Output";
        outputBox.outputChannelIndex = 0;
        g.boxes.push(outputBox);
        const outputId = outputBox.id;

        g._buildFromItems(items, INVALID_BOX_ID);

        for (const b of g.boxes) {
            if (b.id === outputId) continue;
            if (!g.connections.some(c => c.from === b.id)) {
                g.connections.push(new Connection(b.id, outputId));
            }
        }
        return g;
    }

    private _buildFromItems(items: PedalboardItem[], upstreamId: number): void {
        for (const item of items) {
            const box = new Box();
            box.id        = item.instanceId;
            box.isEnabled = item.isEnabled;
            box.pluginUri  = item.uri;
            box.pluginName = item.pluginName ?? "";
            box.title      = item.title      ?? "";
            box.iconColor  = item.iconColor  ?? "";
            box.controlValues = item.controlValues.map(cv => new ControlValue(cv.key, cv.value));

            if (item.isSplit()) {
                box.type = item.getControlValue("splitType") === 0 ? "Ab" : "Merge";
            } else {
                box.type = "Plugin";
            }

            if (box.id >= this.nextBoxId) this.nextBoxId = box.id + 1;
            this.boxes.push(box);

            if (upstreamId !== INVALID_BOX_ID) {
                this.connections.push(new Connection(upstreamId, box.id));
            }

            if (item.isSplit()) {
                const split = item as PedalboardSplitItem;
                this._buildFromItems(split.topChain,    box.id);
                this._buildFromItems(split.bottomChain, box.id);
            } else {
                upstreamId = box.id;
            }
        }
    }
}



export class PedalboardSplitItem extends PedalboardItem {
    static PANL_KEY: string = "panL";
    static PANR_KEY: string = "panR";
    static VOLL_KEY: string = "volL";
    static VOLR_KEY: string = "volR";
    
    static MIX_KEY: string = "mix";
    static TYPE_KEY: string = "splitType";
    static SELECT_KEY: string = "select";


    deserialize(input: any): PedalboardSplitItem {
        this.deserializePedalboardItem(input);
        this.topChain = PedalboardItem.deserializeArray(input.topChain);
        this.bottomChain = PedalboardItem.deserializeArray(input.bottomChain);

        return this;
    }
    getSplitType(): SplitType {
        let rawValue = this.getControlValue(PedalboardSplitItem.TYPE_KEY);

        if (rawValue <  1) return SplitType.Ab;
        if (rawValue < 2) return SplitType.Mix;
        return SplitType.Lr;
    }
    getToggleAbControlValue(): ControlValue {
        let cv = new ControlValue();
        cv.key = "select";
        cv.value = this.isASelected() ? 1 : 0;
        return cv;

    }
    getMixControl(): ControlValue {
        return this.getControl(PedalboardSplitItem.MIX_KEY);
    }
    getMix(): number {
        return this.getControlValue(PedalboardSplitItem.MIX_KEY);
    }
    isASelected(): boolean {
        return this.getSplitType() !== SplitType.Ab ||  this.getControlValue(PedalboardSplitItem.SELECT_KEY) === 0;
    }
    isBSelected(): boolean {
        return this.getSplitType() !== SplitType.Ab || this.getControlValue(PedalboardSplitItem.SELECT_KEY) !== 0;
    }

    topChain: PedalboardItem[] = PedalboardItem.EmptyArray;
    bottomChain: PedalboardItem[] = PedalboardItem.EmptyArray;
}





export class Pedalboard implements Deserializable<Pedalboard> {

    static readonly START_CONTROL = -2; // synthetic PedalboardItem for input volume.
    static readonly END_CONTROL = -3; // synthetic PedalboardItem for output volume.
    static readonly START_PEDALBOARD_ITEM_URI = "uri://two-play/pipedal/pedalboard#Start";
    static readonly END_PEDALBOARD_ITEM_URI = "uri://two-play/pipedal/pedalboard#End";

    deserialize(input: any): Pedalboard {
        this.name = input.name;
        this.input_volume_db  = input.input_volume_db;
        this.output_volume_db = input.output_volume_db;
        this.items = PedalboardItem.deserializeArray(input.items);
        this.nextInstanceId = input.nextInstanceId ?? -1;
        this.snapshots = input.snapshots ? Snapshot.deserializeArray(input.snapshots): [];
        this.selectedSnapshot = input.selectedSnapshot;
        this.pathProperties = input.pathProperties;
        this.selectedPlugin = input.selectedPlugin??-1;
        if (input.routingGraph && input.routingGraph.boxes && input.routingGraph.boxes.length > 0) {
            this.routingGraph = new RoutingGraph().deserialize(input.routingGraph);
        } else {
            this.routingGraph = RoutingGraph.fromItems(this.items, this.name);
        }
        this.parallelChains = (input.parallelChains ?? []).map(
            (chain: any[]) => PedalboardItem.deserializeArray(chain)
        );
        return this;
    }

    clone(): Pedalboard {
        return new Pedalboard().deserialize(this);
    }
    name: string = "";
    input_volume_db: number = 0;
    output_volume_db: number = 0;
    items: PedalboardItem[] = [];
    nextInstanceId: number = -1;

    snapshots: (Snapshot | null)[] = [];
    selectedSnapshot: number = -1;
    pathProperties: {[Name: string]: string} = {};
    selectedPlugin: number = -1;
    routingGraph: RoutingGraph = new RoutingGraph();
    parallelChains: PedalboardItem[][] = [];

    // Exclude routingGraph and parallelChains from JSON sent to server — C++ backend
    // doesn't yet support parallelChains; it would crash on the unknown field.
    toJSON(): object {
        const { routingGraph: _rg, parallelChains: _pc, ...rest } = this as any;
        return rest;
    }

    // yields all items in the pedalboard, including split items. Splits are yielded before their children.
    *itemsGenerator(): Generator<PedalboardItem, void, undefined> {
        let it = itemGenerator_(this.items);
        while (true) {
            let v = it.next();
            if (v.done) break;
            yield v.value;
        }
        for (const chain of this.parallelChains) {
            it = itemGenerator_(chain);
            while (true) {
                let v = it.next();
                if (v.done) break;
                yield v.value;
            }
        }
    }
    // same as itemsGenerator, but yields split items after their chains.
    *itemsGeneratorSplitAfter(): Generator<PedalboardItem, void, undefined> {
        let it = itemGeneratorSplitAfter_(this.items);
        while (true) {
            let v = it.next();
            if (v.done) break;
            yield v.value;
        }
        for (const chain of this.parallelChains) {
            it = itemGeneratorSplitAfter_(chain);
            while (true) {
                let v = it.next();
                if (v.done) break;
                yield v.value;
            }
        }
    }

    makeSnapshot(): Snapshot {
        let result = new Snapshot();
        let it = this.itemsGenerator();
        while (true)
        {
            let v = it.next();
            if (v.done) break;
            let pedalboardItem = v.value;
            let snapshotValue =  SnapshotValue.createFromPedalboardItem(pedalboardItem);
            result.values.push(snapshotValue);
        }
        return result;
    }
    hasItem(instanceId: number): boolean
    {
        let it = this.itemsGenerator();
        while (true)
        {
            let v = it.next();
            if (v.done) break;
            if (v.value.instanceId === instanceId)
            {
                return true;
            }
        }
        return false;

    }

    getFirstSelectableItem(): number
    {
        if (this.items.length !== 0)
        {
            return this.items[0].instanceId;
        }
        return -1;

    }

    maybeGetItem(instanceId: number): PedalboardItem | null{
        let it = this.itemsGenerator();
        while (true)
        {
            let v = it.next();
            if (v.done) break;
            if (v.value.instanceId === instanceId)
            {
                return v.value;
            }
        }
        return null;
    }
    makeStartItem(): PedalboardItem {
        let result = new PedalboardItem();
        result.pluginName = "Input";
        result.instanceId = Pedalboard.START_CONTROL;
        result.uri = Pedalboard.START_PEDALBOARD_ITEM_URI;
        result.isEnabled = true;
        result.controlValues = [new ControlValue("volume_db",this.input_volume_db)];
        return result;

    }
    makeEndItem(): PedalboardItem {
        let result = new PedalboardItem();
        result.pluginName = "Output";
        result.instanceId = Pedalboard.END_CONTROL;
        result.uri = Pedalboard.END_PEDALBOARD_ITEM_URI;
        result.isEnabled = true;
        result.controlValues = [new ControlValue("volume_db",this.output_volume_db)];
        return result;

    }

    getItem(instanceId: number): PedalboardItem {
        let it = this.itemsGenerator();
        while (true)
        {
            let v = it.next();
            if (v.done) break;
            if (v.value.instanceId === instanceId)
            {
                return v.value;
            }
        }
        throw new PiPedalArgumentError("Item not found.");
    }
    tryGetItem(instanceId: number): PedalboardItem | null {
        let it = this.itemsGenerator();
        while (true)
        {
            let v = it.next();
            if (v.done) break;
            if (v.value.instanceId === instanceId)
            {
                return v.value;
            }
        }
        return null;
    }
    private deleteItem_(instanceId: number,items: PedalboardItem[]): number | null
    {
        for (let i = 0; i < items.length; ++i)
        {
            let item = items[i];
            if (item.instanceId === instanceId)
            {
                if (items.length > 1) {
                    items.splice(i,1);
                    let nextSelectedItem = i;
                    if (i >= items.length) --nextSelectedItem;
                    return items[nextSelectedItem].instanceId;
                } else {
                    // replace with an empty item.
                    let newItem = this.createEmptyItem();
                    items[i] = newItem;
                    return newItem.instanceId;

                }
            } else {
                if (item.isSplit())
                {
                    let splitItem = item as PedalboardSplitItem;
                    let t = this.deleteItem_(instanceId,splitItem.topChain);
                    if (t != null) return t;

                    t = this.deleteItem_(instanceId,splitItem.bottomChain);
                    if (t != null) return t;
                }
            }
        }
        return null;
    }

    canDeleteItem_(instanceId: number, items: PedalboardItem[], insideSplit: boolean = false): boolean
    {
        for (let i = 0; i < items.length; ++i)
        {
            let item = items[i];
            if (item.instanceId === instanceId)
            {
                if (items.length > 1) return true;
                if (insideSplit) return true; // single item in a split branch — will collapse the split
                return !item.isEmpty();
            }
            if (item.isSplit())
            {
                let splitItem = item as PedalboardSplitItem;
                if (this.canDeleteItem_(instanceId, splitItem.topChain, true)) return true;
                if (this.canDeleteItem_(instanceId, splitItem.bottomChain, true)) return true;
            }
        }
        return false;
    }

    private collapseSplitContaining_(instanceId: number, items: PedalboardItem[]): number | null
    {
        for (let i = 0; i < items.length; ++i)
        {
            const item = items[i];
            if (!item.isSplit()) continue;
            const split = item as PedalboardSplitItem;

            let keepChain: PedalboardItem[] | null = null;
            if (split.topChain.length === 1 && split.topChain[0].instanceId === instanceId) {
                keepChain = split.bottomChain;
            } else if (split.bottomChain.length === 1 && split.bottomChain[0].instanceId === instanceId) {
                keepChain = split.topChain;
            }

            if (keepChain !== null) {
                items.splice(i, 1, ...keepChain);
                return keepChain[0]?.instanceId ?? -1;
            }

            let t = this.collapseSplitContaining_(instanceId, split.topChain);
            if (t !== null) return t;
            t = this.collapseSplitContaining_(instanceId, split.bottomChain);
            if (t !== null) return t;
        }
        return null;
    }

    collapseSplitContaining(instanceId: number): number
    {
        let result = this.collapseSplitContaining_(instanceId, this.items);
        if (result !== null && result !== -1) return result;
        for (const chain of this.parallelChains) {
            result = this.collapseSplitContaining_(instanceId, chain);
            if (result !== null && result !== -1) return result;
        }
        // fallback: just replace with empty
        const newItem = this.createEmptyItem();
        this.items = [newItem];
        return newItem.instanceId;
    }

    isOnlyItemInSplitBranch(instanceId: number): boolean
    {
        if (this.isOnlyItemInSplitBranch_(instanceId, this.items)) return true;
        for (const chain of this.parallelChains) {
            if (this.isOnlyItemInSplitBranch_(instanceId, chain)) return true;
        }
        return false;
    }

    isOnlyItemInParallelChain(instanceId: number): boolean
    {
        for (const chain of this.parallelChains) {
            if (chain.length === 1 && chain[0].instanceId === instanceId) return true;
        }
        return false;
    }

    removeParallelChainContaining(instanceId: number): number
    {
        const fallback = this.items[0]?.instanceId ?? -1;
        for (let i = 0; i < this.parallelChains.length; ++i) {
            if (this.parallelChains[i].some(item => item.instanceId === instanceId)) {
                this.parallelChains.splice(i, 1);
                return fallback;
            }
        }
        return fallback;
    }

    addParallelChain(): number
    {
        const emptyItem = this.createEmptyItem();
        this.parallelChains.push([emptyItem]);
        return emptyItem.instanceId;
    }

    private isOnlyItemInSplitBranch_(instanceId: number, items: PedalboardItem[]): boolean
    {
        for (const item of items) {
            if (!item.isSplit()) continue;
            const split = item as PedalboardSplitItem;
            if (split.topChain.length === 1 && split.topChain[0].instanceId === instanceId) return true;
            if (split.bottomChain.length === 1 && split.bottomChain[0].instanceId === instanceId) return true;
            if (this.isOnlyItemInSplitBranch_(instanceId, split.topChain)) return true;
            if (this.isOnlyItemInSplitBranch_(instanceId, split.bottomChain)) return true;
        }
        return false;
    }

    canDeleteItem(instanceId: number): boolean
    {
        if (this.isOnlyItemInParallelChain(instanceId)) return true;
        if (this.canDeleteItem_(instanceId, this.items)) return true;
        for (const chain of this.parallelChains) {
            if (this.canDeleteItem_(instanceId, chain)) return true;
        }
        return false;
    }
    // Returns the next selected instanceId, or null if no deletion occurred.
    deleteItem(instanceId: number): number | null {
        let result = this.deleteItem_(instanceId, this.items);
        if (result !== null) return result;
        for (const chain of this.parallelChains) {
            result = this.deleteItem_(instanceId, chain);
            if (result !== null) return result;
        }
        return null;
    }

    setMidiBinding(instanceId: number, midiBinding: MidiBinding): boolean
    {
        let item = this.getItem(instanceId);
        if (!item) return false;
        return item.setMidiBinding(midiBinding);
    }
    addToStart(item: PedalboardItem)
    {
        this.items.splice(0,0,item);
    }
    addToEnd(item: PedalboardItem)
    {
        this.items.splice(this.items.length,0,item);
    }
    static _addRelative(items: PedalboardItem[],newItem: PedalboardItem, instanceId: number, addBefore: boolean): boolean
    {
        for (let i = 0; i < items.length; ++i)
        {
            let item = items[i];
            if (item.instanceId === instanceId)
            {
                if (addBefore)
                {
                    items.splice(i,0,newItem);
                } else {
                    items.splice(i+1,0,newItem);
                }
                return true;
            }
            if (item.isSplit())
            {
                let split = item as PedalboardSplitItem;
                if (this._addRelative(split.topChain,newItem,instanceId,addBefore))
                {
                    return true;
                }
                if (this._addRelative(split.bottomChain,newItem,instanceId,addBefore))
                {
                    return true;
                }
            }
        }
        return false;

    }

    addBefore(item: PedalboardItem, instanceId: number)
    {
        if (item.instanceId === instanceId) return;
        let result = Pedalboard._addRelative(this.items, item, instanceId, true);
        if (!result) {
            for (const chain of this.parallelChains) {
                result = Pedalboard._addRelative(chain, item, instanceId, true);
                if (result) break;
            }
        }
        if (!result) throw new PiPedalArgumentError("instanceId not found.");
    }
    addAfter(item: PedalboardItem, instanceId: number)
    {
        if (item.instanceId === instanceId) return;
        let result = Pedalboard._addRelative(this.items, item, instanceId, false);
        if (!result) {
            for (const chain of this.parallelChains) {
                result = Pedalboard._addRelative(chain, item, instanceId, false);
                if (result) break;
            }
        }
        if (!result) throw new PiPedalArgumentError("instanceId not found.");
    }
    
    ensurePedalboardIds() {
        if (this.nextInstanceId === -1) {
            let minId = 1;
            let it = this.itemsGenerator();
            while (true) {
                let v = it.next();
                if (v.done) break;
                if (v.value.instanceId) {
                    let t = v.value.instanceId;
                    if (t+1 > minId) minId = t+1;
                }
    
            }
            this.nextInstanceId = minId;
        }
        let it = this.itemsGenerator();
        while (true) {
            let v = it.next();
            if (v.done) break;
            if (v.value.instanceId === -1) {
                v.value.instanceId = ++this.nextInstanceId;
    
            }
        }
    }
    createEmptySplit(): PedalboardSplitItem {
        let result: PedalboardSplitItem = new PedalboardSplitItem();
        result.uri = SPLIT_PEDALBOARD_ITEM_URI;
        result.instanceId = ++this.nextInstanceId;
        result.pluginName = "";
        result.isEnabled = true;
        result.topChain = [ this.createEmptyItem()];
        result.bottomChain = [ this.createEmptyItem()];
        result.controlValues = [
            new ControlValue(PedalboardSplitItem.TYPE_KEY, 1),
            new ControlValue(PedalboardSplitItem.SELECT_KEY,0),
            new ControlValue(PedalboardSplitItem.MIX_KEY,0),
            new ControlValue(PedalboardSplitItem.PANL_KEY,0),
            new ControlValue(PedalboardSplitItem.VOLL_KEY,0),
            new ControlValue(PedalboardSplitItem.PANR_KEY,0),
            new ControlValue(PedalboardSplitItem.VOLR_KEY,0)
        ];


        return result;


    }
    createEmptyItem(): PedalboardItem {
        let result: PedalboardItem = new PedalboardItem();
        result.uri = EMPTY_PEDALBOARD_ITEM_URI;
        result.instanceId = ++this.nextInstanceId;
        result.pluginName = "";
        result.isEnabled = true;

        return result;
    }
    setItemEmpty(item: PedalboardItem)
    {
        item.uri = EMPTY_PEDALBOARD_ITEM_URI;
        item.instanceId = ++this.nextInstanceId;
        item.pluginName = "";
        item.isEnabled = true;
        item.controlValues = [];
        item.vstState = "";
        item.lv2State = [false,{}];

    }

    _replaceItem(items: PedalboardItem[], instanceId: number, newItem: PedalboardItem): boolean {
        for (let i = 0; i < items.length; ++i)
        {
            let item = items[i];
            if (item.instanceId === instanceId)
            {
                items[i] = newItem;
                return true;
            }
            if (items[i].isSplit())
            {
                let  splitItem = item as PedalboardSplitItem;
                if (this._replaceItem(splitItem.topChain,instanceId,newItem))
                    return true;
                if (this._replaceItem(splitItem.bottomChain,instanceId,newItem))
                {
                    return true;
                }
            }
        }
        return false;
    }
    
    replaceItem(instanceId: number, newItem: PedalboardItem)
    {
        let result = this._replaceItem(this.items, instanceId, newItem);
        if (!result) {
            for (const chain of this.parallelChains) {
                result = this._replaceItem(chain, instanceId, newItem);
                if (result) break;
            }
        }
        if (!result) throw new PiPedalArgumentError("instanceId not found.");
    }
    private _addItem(items: PedalboardItem[], newItem: PedalboardItem, instanceId: number, append: boolean)
    {
        for (let i = 0; i < items.length; ++i)
        {
            let item = items[i];
            if (item.instanceId === instanceId)
            {
                if (append)
                {
                    items.splice(i+1,0,newItem);
                    return true;
                } else {
                    items.splice(i,0,newItem);
                    return true;
                }
            }
            if (item.isSplit())
            {
                let splitItem = item as PedalboardSplitItem;
                if (this._addItem(splitItem.topChain,newItem,instanceId,append)) return true;
                if (this._addItem(splitItem.bottomChain,newItem,instanceId,append)) return true;
            }
        }
    }

    addItem(newItem: PedalboardItem, instanceId: number, append: boolean): void
    {
        if (this._addItem(this.items, newItem, instanceId, append)) return;
        for (const chain of this.parallelChains) {
            if (this._addItem(chain, newItem, instanceId, append)) return;
        }
    }

}

function* itemGenerator_(items: PedalboardItem[]): Generator<PedalboardItem, void, undefined> {
    for (let i = 0; i < items.length; ++i) {
        let item = items[i];
        yield item;
        if (item.uri === SPLIT_PEDALBOARD_ITEM_URI) {

            let splitItem = item as PedalboardSplitItem;

            let it = itemGenerator_(splitItem.topChain);
            while (true) {
                let v = it.next();
                if (v.done) break;
                yield v.value;
            }
            it = itemGenerator_(splitItem.bottomChain);
            while (true) {
                let v = it.next();
                if (v.done) break;
                yield v.value;
            }
        }
    }
}

function* itemGeneratorSplitAfter_(items: PedalboardItem[]): Generator<PedalboardItem, void, undefined> {
    for (let i = 0; i < items.length; ++i) {
        let item = items[i];
        if (item.uri === SPLIT_PEDALBOARD_ITEM_URI) {

            let splitItem = item as PedalboardSplitItem;

            let it = itemGenerator_(splitItem.topChain);
            while (true) {
                let v = it.next();
                if (v.done) break;
                yield v.value;
            }
            it = itemGenerator_(splitItem.bottomChain);
            while (true) {
                let v = it.next();
                if (v.done) break;
                yield v.value;
            }
        }
        yield item;
    }
}


