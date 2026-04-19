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

#include "pch.h"
#include "Pedalboard.hpp"
#include "AtomConverter.hpp"
#include "PluginHost.hpp"
#include "AtomConverter.hpp"
#include "SplitEffect.hpp"


using namespace pipedal;


// ---------------------------------------------------------------------------
// US-04: Traversal via RoutingGraph.
//
// GetItem and GetAllPlugins now delegate to the RoutingGraph. The legacy
// static helpers (GetItem_, GetAllItems) are kept but no longer called by
// the public API — they remain available for the DSP path (Lv2Pedalboard)
// which still uses the old model until US-07.
// ---------------------------------------------------------------------------

static const PedalboardItem* GetItem_(const std::vector<PedalboardItem>&items,int64_t pedalboardItemId)
{
    for (size_t i = 0; i < items.size(); ++i)
    {
        auto &item = items[i];
        if (items[i].instanceId() == pedalboardItemId)
        {
            return &(items[i]);
        }
        if (item.isSplit())
        {
            const PedalboardItem* t = GetItem_(item.topChain(),pedalboardItemId);
            if (t != nullptr) return t;
            t = GetItem_(item.bottomChain(),pedalboardItemId);
            if (t != nullptr) return t;
        }
    }
    return nullptr;
}

static void GetAllItems(std::vector<PedalboardItem*> & result, std::vector<PedalboardItem>&items)
{
    for (auto& item: items)
    {
        if (item.isSplit())
        {
            GetAllItems(result,item.topChain());
            GetAllItems(result,item.bottomChain());
        }
        result.push_back(&item);
    }    
}

// GetAllPlugins: returns all PedalboardItems by walking items_ directly.
// The RoutingGraph is used for topology decisions; PedalboardItem* pointers
// must still point into items_ since the rest of the codebase writes through them.
std::vector<PedalboardItem*> Pedalboard::GetAllPlugins()
{
    std::vector<PedalboardItem*> result;
    GetAllItems(result, this->items());
    return result;
}

// GetItem: still walks items_ to return a live pointer.
const PedalboardItem* Pedalboard::GetItem(int64_t pedalItemId) const
{
    return GetItem_(this->items(), pedalItemId);
}
PedalboardItem* Pedalboard::GetItem(int64_t pedalItemId)
{
    return const_cast<PedalboardItem*>(GetItem_(this->items(), pedalItemId));
}


ControlValue* PedalboardItem::GetControlValue(const std::string&symbol)
{
    for (size_t i = 0; i < this->controlValues().size(); ++i)
    {
        if (this->controlValues()[i].key() == symbol)
        {
            return &(this->controlValues()[i]);
        }
    }
    return nullptr;
}



bool PedalboardItem::SetControlValue(const std::string&symbol, float value)
{
    ControlValue*controlValue = GetControlValue(symbol);
    if (controlValue == nullptr) 
    {
        this->controlValues().push_back(ControlValue(symbol.c_str(),value));
        return true;
    }
    if (controlValue->value() != value)
    {
        controlValue->value(value);
        return true;
    }
    return false;
}

const ControlValue* PedalboardItem::GetControlValue(const std::string&symbol) const
{
    for (size_t i = 0; i < this->controlValues().size(); ++i)
    {
        if (this->controlValues()[i].key() == symbol)
        {
            return &(this->controlValues()[i]);
        }
    }
    return nullptr;
}

bool Pedalboard::SetItemUseModUi(int64_t pedalItemId, bool enabled)
{
    PedalboardItem*item = GetItem(pedalItemId);
    if (!item) return false;
    if (item->useModUi() != enabled)
    {
        item->useModUi(enabled);
        return true;
    }
    return false;

}
bool Pedalboard::SetItemEnabled(int64_t pedalItemId, bool enabled)
{
    PedalboardItem*item = GetItem(pedalItemId);

    if (!item) return false;
    if (item->isEnabled() != enabled)
    {
        item->isEnabled(enabled);
        return true;
    }
    return false;

}


bool Pedalboard::SetControlValue(int64_t pedalItemId, const std::string &symbol, float value)
{
    PedalboardItem*item = GetItem(pedalItemId);
    if (!item) return false;
    return item->SetControlValue(symbol,value);
}

bool Pedalboard::SetItemTitle(int64_t pedalItemId, const std::string &title, const std::string&iconColor)
{
    PedalboardItem*item = GetItem(pedalItemId);
    if (!item) return false;
    if (item->title() == title && item->iconColor() == iconColor) return false; // no change.
    item->title(title);
    item->iconColor(iconColor);
    return true;
}


PedalboardItem Pedalboard::MakeEmptyItem()
{
    uint64_t instanceId = NextInstanceId();

    PedalboardItem result;
    result.instanceId(instanceId);
    result.uri(EMPTY_PEDALBOARD_ITEM_URI);
    result.pluginName("");
    result.isEnabled(true);
    return result;
}


PedalboardItem Pedalboard::MakeSplit()
{
    uint64_t instanceId = NextInstanceId();

    PedalboardItem result;
    result.instanceId(instanceId);
    result.uri(SPLIT_PEDALBOARD_ITEM_URI);
    result.pluginName("");
    result.isEnabled(true);

    result.topChain().push_back(MakeEmptyItem());
    result.bottomChain().push_back(MakeEmptyItem());
    result.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY,0));
    result.controlValues().push_back(ControlValue(SPLIT_SELECT_KEY,0));
    result.controlValues().push_back(ControlValue(SPLIT_MIX_KEY,0));
    result.controlValues().push_back(ControlValue(SPLIT_PANL_KEY,0));
    result.controlValues().push_back(ControlValue(SPLIT_VOLL_KEY,-3));
    result.controlValues().push_back(ControlValue(SPLIT_PANR_KEY,0));
    result.controlValues().push_back(ControlValue(SPLIT_VOLR_KEY,-3));
    
    return result;
}



Pedalboard Pedalboard::MakeDefault()
{
    // copy insanity. but it happens so rarely.
    Pedalboard result;

    result.items().push_back(result.MakeEmptyItem());
    result.name("Default Preset");
    result.MarkRoutingGraphDirty(); // US-03: graph will be rebuilt on first access.
    return result;
}


bool IsPedalboardSplitItem(const PedalboardItem*self, const std::vector<PedalboardItem>&value)
{
    return self->uri() == SPLIT_PEDALBOARD_ITEM_URI;
}

// ---------------------------------------------------------------------------
// US-03: RoutingGraph builder
// ---------------------------------------------------------------------------

void Pedalboard::BuildRoutingGraphFromItems(
    const std::vector<PedalboardItem>& items,
    BoxId upstreamId)
{
    for (const auto& item : items)
    {
        auto box = std::make_shared<Box>();
        box->id         = item.instanceId();
        box->isEnabled  = item.isEnabled();
        box->pluginUri  = item.uri();
        box->pluginName = item.pluginName();
        box->title      = item.title();
        box->iconColor  = item.iconColor();

        if (item.isSplit())
        {
            auto cv = item.GetControlValue(SPLIT_SPLITTYPE_KEY);
            float splitTypeVal = cv ? cv->value() : 0.0f;
            box->type = (splitTypeVal == 0.0f) ? BoxType::Ab : BoxType::Merge;
        }
        else if (item.uri() == OUTPUT_PEDALBOARD_ITEM_URI)
        {
            box->type = BoxType::Output;
        }
        else
        {
            box->type = BoxType::Plugin;
        }

        for (const auto& cv : item.controlValues())
        {
            box->controlValues.emplace_back(cv.key(), cv.value());
        }

        if (box->id >= routingGraph_.nextBoxId_)
            routingGraph_.nextBoxId_ = box->id + 1;

        routingGraph_.boxes_.push_back(box);

        if (upstreamId != INVALID_BOX_ID)
            routingGraph_.Connect(upstreamId, box->id);

        if (item.isSplit())
        {
            BuildRoutingGraphFromItems(item.topChain(),    box->id);
            BuildRoutingGraphFromItems(item.bottomChain(), box->id);
        }
        else
        {
            upstreamId = box->id;
        }
    }
}

void Pedalboard::RebuildRoutingGraph()
{
    // US-06: If the graph was loaded from JSON it already has boxes — use it
    // directly instead of rebuilding from items_.
    if (!routingGraph_.boxes_.empty())
    {
        routingGraphDirty_ = false;
        return;
    }

    routingGraph_ = RoutingGraph();
    routingGraph_.name = name_;

    // Check whether items_ contains any explicit #Output boxes.
    std::function<bool(const std::vector<PedalboardItem>&)> hasOutputItem;
    hasOutputItem = [&](const std::vector<PedalboardItem>& items) -> bool {
        for (const auto& i : items) {
            if (i.uri() == OUTPUT_PEDALBOARD_ITEM_URI) return true;
            if (i.isSplit() &&
                (hasOutputItem(i.topChain()) || hasOutputItem(i.bottomChain())))
                return true;
        }
        return false;
    };

    BoxId outputId = INVALID_BOX_ID;
    if (!hasOutputItem(items_))
    {
        auto outputBox = routingGraph_.MakeOutputBox(0);
        outputBox->title = "Output";
        routingGraph_.boxes_.push_back(outputBox);
        outputId = outputBox->id;
    }

    BuildRoutingGraphFromItems(items_, INVALID_BOX_ID);

    // Connect terminal plugin/merge/ab nodes to the implicit output (if one was created).
    if (outputId != INVALID_BOX_ID)
    {
        for (const auto& b : routingGraph_.boxes_)
        {
            if (b->id == outputId) continue;
            if (b->isOutput()) continue;
            if (routingGraph_.OutgoingCount(b->id) == 0)
                routingGraph_.Connect(b->id, outputId);
        }
    }

    routingGraphDirty_ = false;
}


bool Pedalboard::ApplySnapshot(int64_t snapshotIndex, PluginHost&pluginHost)
{
    if (snapshotIndex < 0 || 
        snapshotIndex >= this->snapshots_.size() || 
        this->snapshots_[snapshotIndex] == nullptr 
    )
    {
        return false;
    }
    std::map<int64_t, SnapshotValue*> indexedValues;
    Snapshot *snapshot = this->snapshots_[snapshotIndex].get();

    for (auto &value: snapshot->values_)
    {
        indexedValues[value.instanceId_] = &value;
    }

    auto plugins = this->GetAllPlugins();
    for (PedalboardItem *pedalboardItem: plugins)
    {
        if (!pedalboardItem->isEmpty())
        {
            SnapshotValue*snapshotValue = indexedValues[pedalboardItem->instanceId()];
            if (snapshotValue)
            {
                pedalboardItem->ApplySnapshotValue(snapshotValue);
            } else {
                pedalboardItem->ApplyDefaultValues(pluginHost);
            }
        }
    }
    return true;
}

void PedalboardItem::ApplyDefaultValues(PluginHost&pluginHost)
{
    if (isEmpty()) return;
    auto pluginInfo = pluginHost.GetPluginInfo(this->uri());
    if (!pluginInfo)
    {
        if (this->isSplit())
        {
            pluginInfo = GetSplitterPluginInfo();
        }
    }
    this->isEnabled(true);
    if (pluginInfo)
    {
        for (auto &port: pluginInfo->ports())
        {
            this->SetControlValue(port->symbol(),port->default_value());
        }
        for (auto &pathProperty: this->pathProperties_)
        {
            pathProperties_[pathProperty.first] = AtomConverter::EmptyPathstring();
        }
    }
}


void PedalboardItem::ApplySnapshotValue(SnapshotValue*snapshotValue)
{
    std::map<std::string,float> cumulativeValues;
    for (auto &controlValue: this->controlValues())
    {
        cumulativeValues[controlValue.key()] = controlValue.value();
    }
    for (auto&controlValue : snapshotValue->controlValues_)
    {
        cumulativeValues[controlValue.key()] = controlValue.value();
    }
    this->controlValues().clear();
    for (auto&pair: cumulativeValues)
    {
        this->controlValues_.push_back(ControlValue(pair.first.c_str(),pair.second));
    }
    if (this->lv2State() != snapshotValue->lv2State_)
    {
        this->lv2State(snapshotValue->lv2State_);
        this->stateUpdateCount(this->stateUpdateCount()+1);
    }
    for (auto&property: snapshotValue->pathProperties_)
    {
        if (property.second == "null")
        {
            this->pathProperties_[property.first] = AtomConverter::EmptyPathstring();
        } else {
            this->pathProperties_[property.first] = property.second;
        }
    }
    this->isEnabled(snapshotValue->isEnabled_);
}


// can we just send a snapshot-style update instead of reloading plugins? All settings are ignored.
bool Pedalboard::IsStructureIdentical(const Pedalboard &other) const
{
    if (this->nextInstanceId_ != other.nextInstanceId_)
    {
        return false;
    }
    if (this->items_.size() != other.items_.size()) 
    {
        return false;
    }
    for (size_t i = 0; i < this->items_.size();++i) 
    {
        if (!this->items_[i].IsStructurallyIdentical(other.items_[i]))
        {
            return false;
        }
    }
    return true;
}

bool PedalboardItem::IsStructurallyIdentical(const PedalboardItem&other) const
{
    if (this->instanceId() != other.instanceId())
    {
        return false;
    }
    if (this->uri() != other.uri())
    {
        return false;
    }
    if (this->midiBindings() != other.midiBindings())
    {
        return false;
    }
    if (this->isSplit())
    {
        auto myValue = this->GetControlValue("splitType");
        auto otherValue = other.GetControlValue("splitType");
        if (myValue == nullptr || otherValue == nullptr)
        {
            return false;
        }
        if (myValue->value() != otherValue->value())
        {
            return false; 
        }
        if (topChain().size() != other.topChain().size())
        {
            return false;
        }
        for (size_t i = 0; i < topChain().size(); ++i)
        {
            if (!topChain()[i].IsStructurallyIdentical(other.topChain()[i]))
            {
                return false;
            }
        }
        // FIX: size-mismatch guard and item loop were previously inverted.
        if (bottomChain().size() != other.bottomChain().size())
        {
            return false;
        }
        for (size_t i = 0; i < bottomChain().size(); ++i)
        {
            if (!bottomChain()[i].IsStructurallyIdentical(other.bottomChain()[i]))
            {
                return false;
            }
        }
    }
    return true;
}

void PedalboardItem::AddToSnapshotFromCurrentSettings(Snapshot&snapshot) const
{
    SnapshotValue snapshotValue;
    snapshotValue.instanceId_ = this->instanceId_;
    snapshotValue.isEnabled_ = this->isEnabled_;

    for (const ControlValue &value: this->controlValues_)
    {
        snapshotValue.controlValues_.push_back(value);
    }
    for (const auto&pathProperty: this->pathProperties_)
    {
        snapshotValue.pathProperties_[pathProperty.first] = pathProperty.second;
    }
    snapshotValue.lv2State_ = this->lv2State_;
    snapshot.values_.push_back(std::move(snapshotValue));

    if (this->isSplit())
    {
        for (auto&item: this->topChain_)
        {
            item.AddToSnapshotFromCurrentSettings(snapshot);
        }
        for (auto&item: this->bottomChain_)
        {
            item.AddToSnapshotFromCurrentSettings(snapshot);
        }
    }
}

void PedalboardItem::AddResetsForMissingProperties(Snapshot&snapshot, size_t*index) const
{
    SnapshotValue&snapshotValue = snapshot.values_[*index];
    
    if (snapshotValue.instanceId_ != this->instanceId())
    {
        throw std::runtime_error("Pedalboard structure does not match.");
    }

    for (auto&property: this->pathProperties_)
    {
        auto f = snapshotValue.pathProperties_.find(property.first);
        if (f == snapshotValue.pathProperties_.end())
        {
            snapshotValue.pathProperties_[property.first] = AtomConverter::EmptyPathstring();
        }
    }   
    ++(*index);

    if (this->isSplit())
    {
        for (auto&item: this->topChain())
        {
            item.AddResetsForMissingProperties(snapshot,index);
        }
        for (auto&item: this->bottomChain())
        {
            item.AddResetsForMissingProperties(snapshot,index);
        }
    }
}

Pedalboard Pedalboard::DeepCopy()
{
    Pedalboard result = *this;
    for (size_t i= 0; i < snapshots_.size(); ++i)
    {
        if (snapshots_[i])
        {
            result.snapshots_[i] = std::make_shared<Snapshot>(*(snapshots_[i]));
        }
    }
    result.MarkRoutingGraphDirty(); // US-03: force graph rebuild in the copy.
    return result;
}

void Pedalboard::SetCurrentSnapshotModified(bool modified)
{
    if (selectedSnapshot() != -1)
    {
        auto& snapshot = snapshots_[selectedSnapshot_];
        if (snapshot)
        {
            snapshot->isModified_ = modified;
        }
    }
}

Snapshot Pedalboard::MakeSnapshotFromCurrentSettings(const Pedalboard &previousPedalboard)
{
    Snapshot snapshot;
    auto items = this->GetAllPlugins();
    for (auto item : items)
    {
        item->AddToSnapshotFromCurrentSettings(snapshot);
    }
    return snapshot;
}



JSON_MAP_BEGIN(ControlValue)
    JSON_MAP_REFERENCE(ControlValue,key)
    JSON_MAP_REFERENCE(ControlValue,value)
JSON_MAP_END()



JSON_MAP_BEGIN(PedalboardItem)
    JSON_MAP_REFERENCE(PedalboardItem,instanceId)
    JSON_MAP_REFERENCE(PedalboardItem,uri)
    JSON_MAP_REFERENCE(PedalboardItem,isEnabled)
    JSON_MAP_REFERENCE(PedalboardItem,controlValues)
    JSON_MAP_REFERENCE(PedalboardItem,pluginName)
    JSON_MAP_REFERENCE_CONDITIONAL(PedalboardItem,topChain,IsPedalboardSplitItem)
    JSON_MAP_REFERENCE_CONDITIONAL(PedalboardItem,bottomChain,&IsPedalboardSplitItem)
    JSON_MAP_REFERENCE(PedalboardItem,midiBindings)
    JSON_MAP_REFERENCE(PedalboardItem,midiChannelBinding)
    JSON_MAP_REFERENCE(PedalboardItem,stateUpdateCount)
    JSON_MAP_REFERENCE(PedalboardItem,lv2State)
    JSON_MAP_REFERENCE(PedalboardItem,lilvPresetUri)
    JSON_MAP_REFERENCE(PedalboardItem,pathProperties)
    JSON_MAP_REFERENCE(PedalboardItem,title)
    JSON_MAP_REFERENCE(PedalboardItem,useModUi)
    JSON_MAP_REFERENCE(PedalboardItem,iconColor)
    JSON_MAP_REFERENCE(PedalboardItem,sideChainInputId)
JSON_MAP_END()


JSON_MAP_BEGIN(Pedalboard)
    JSON_MAP_REFERENCE(Pedalboard,name)
    JSON_MAP_REFERENCE(Pedalboard,input_volume_db)
    JSON_MAP_REFERENCE(Pedalboard,output_volume_db)
    JSON_MAP_REFERENCE(Pedalboard,items)
    JSON_MAP_REFERENCE(Pedalboard,nextInstanceId)
    JSON_MAP_REFERENCE(Pedalboard,snapshots)
    JSON_MAP_REFERENCE(Pedalboard,selectedSnapshot)
    JSON_MAP_REFERENCE(Pedalboard,selectedPlugin)
    JSON_MAP_REFERENCE(Pedalboard,routingGraph)
JSON_MAP_END()

JSON_MAP_BEGIN(SnapshotValue)
    JSON_MAP_REFERENCE(SnapshotValue,instanceId)
    JSON_MAP_REFERENCE(SnapshotValue,isEnabled)
    JSON_MAP_REFERENCE(SnapshotValue,controlValues)
    JSON_MAP_REFERENCE(SnapshotValue,lv2State)
    JSON_MAP_REFERENCE(SnapshotValue,pathProperties)
JSON_MAP_END()

JSON_MAP_BEGIN(Snapshot)
    JSON_MAP_REFERENCE(Snapshot,name)
    JSON_MAP_REFERENCE(Snapshot,isModified)
    JSON_MAP_REFERENCE(Snapshot,color)
    JSON_MAP_REFERENCE(Snapshot,values)
JSON_MAP_END()
