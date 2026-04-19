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

// ---------------------------------------------------------------------------
// RoutingGraph.hpp
//
// US-01 — New routing domain model
// US-02 — Stable Box identity and traversal helpers
//
// Architecture:
//   - Every Box has a stable BoxId (int64_t).
//   - Signal flow is described by explicit directed Connections between Boxes.
//   - Fan-out (split) is implicit: a Box with multiple outgoing connections
//     copies its output to all downstream Boxes.
//   - Fan-in (merge) is implicit: a Box with multiple incoming connections
//     receives the sum of all upstream signals, automatically compensated by
//     -20*log10(N) dB where N is the incoming connection count.
//   - MergeBox and AbBox are explicit only when the user needs manual control
//     over how signals combine.
//   - OutputBox maps to a physical hardware output channel (0-based index).
//   - A branch that does not reach any OutputBox (directly or indirectly) is
//     a dead branch — flagged as a warning, not an error.
//
// This file is backend-only. No runtime wiring, no serialization cutover,
// no DSP changes. Pedalboard.hpp / Lv2Pedalboard are untouched.
// ---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>
#include <cmath>
#include <stdexcept>
#include "json.hpp"

namespace pipedal {

// ---------------------------------------------------------------------------
// BoxId — stable identity for every Box in the graph.
// ---------------------------------------------------------------------------

using BoxId = int64_t;
static constexpr BoxId INVALID_BOX_ID = 0;

// ---------------------------------------------------------------------------
// BoxType
// ---------------------------------------------------------------------------

enum class BoxType
{
    Plugin,   // Any LV2 / VST plugin, including built-in utility plugins.
    Merge,    // Explicit N-to-1 mix with user-controlled levels/pan.
    Ab,       // Explicit N-to-1 switcher — selects one incoming signal.
    Output,   // Terminal: maps to a physical hardware output channel.
    Input,    // Source: exposes physical hardware input channel(s).
};

// ---------------------------------------------------------------------------
// ControlValue (mirrored from Pedalboard.hpp for independence)
// ---------------------------------------------------------------------------

struct RgControlValue : public JsonSerializable
{
    std::string key;
    float       value = 0.0f;

    RgControlValue() = default;
    RgControlValue(const char* k, float v) : key(k), value(v) {}
    RgControlValue(std::string k, float v) : key(std::move(k)), value(v) {}

    void write_json(json_writer& writer) const override;
    void read_json(json_reader& reader) override;
};

// ---------------------------------------------------------------------------
// Box — base for every node in the routing graph.
// ---------------------------------------------------------------------------

class Box : public JsonSerializable
{
public:
    BoxId       id   = INVALID_BOX_ID;
    BoxType     type = BoxType::Plugin;
    bool        isEnabled = true;

    // Human-readable label (optional, for UI).
    std::string title;
    std::string iconColor;

    // Plugin-specific fields (valid when type == BoxType::Plugin).
    std::string pluginUri;
    std::string pluginName;
    std::vector<RgControlValue> controlValues;

    // Output-specific fields (valid when type == BoxType::Output).
    int outputChannelIndex = 0;   // 0 = Out 1, 1 = Out 2, etc.

    // Input-specific fields (valid when type == BoxType::Input).
    int inputChannelCount = 1;    // 1 = mono, 2 = stereo

    // Merge/Ab-specific: user-controlled mix parameters.
    // For implicit fan-in these are ignored; compensation is automatic.
    float mergeVolume = 0.0f;     // dB, relative to compensated sum
    float mergePanL   = 0.0f;
    float mergePanR   = 0.0f;

    // Ab switcher: which input index is currently selected.
    int abSelectedInput = 0;

    Box() = default;
    virtual ~Box() = default;

    void write_json(json_writer& writer) const override;
    void read_json(json_reader& reader) override;

    bool isPlugin() const { return type == BoxType::Plugin; }
    bool isMerge()  const { return type == BoxType::Merge;  }
    bool isAb()     const { return type == BoxType::Ab;     }
    bool isOutput() const { return type == BoxType::Output; }
    bool isInput()  const { return type == BoxType::Input;  }

    RgControlValue* GetControlValue(const std::string& symbol)
    {
        for (auto& cv : controlValues)
            if (cv.key == symbol) return &cv;
        return nullptr;
    }
    const RgControlValue* GetControlValue(const std::string& symbol) const
    {
        for (const auto& cv : controlValues)
            if (cv.key == symbol) return &cv;
        return nullptr;
    }
    bool SetControlValue(const std::string& symbol, float value)
    {
        auto* cv = GetControlValue(symbol);
        if (!cv) { controlValues.emplace_back(symbol, value); return true; }
        if (cv->value != value) { cv->value = value; return true; }
        return false;
    }
};

// ---------------------------------------------------------------------------
// Connection — a directed edge from one Box output to one Box input.
// ---------------------------------------------------------------------------

struct Connection : public JsonSerializable
{
    BoxId from = INVALID_BOX_ID;  // upstream Box
    BoxId to   = INVALID_BOX_ID;  // downstream Box

    Connection() = default;
    Connection(BoxId f, BoxId t) : from(f), to(t) {}

    bool operator==(const Connection& o) const
    {
        return from == o.from && to == o.to;
    }

    void write_json(json_writer& writer) const override;
    void read_json(json_reader& reader) override;
};

// ---------------------------------------------------------------------------
// RoutingGraph — the top-level container.
//
// Ownership: Boxes are owned by the graph via shared_ptr.
// Connections are stored as a flat list of directed edges.
// ---------------------------------------------------------------------------

class RoutingGraph : public JsonSerializable
{
public:
    // US-02: stable identity counter — never reuses IDs within a graph.
    int64_t nextBoxId_ = 1;

    std::vector<std::shared_ptr<Box>>  boxes_;
    std::vector<Connection>            connections_;

    // Human-readable name (mirrors Pedalboard::name_).
    std::string name;

    // ---------------------------------------------------------------------------
    // Factory helpers
    // ---------------------------------------------------------------------------

    BoxId NextBoxId() { return nextBoxId_++; }

    std::shared_ptr<Box> MakePluginBox(const std::string& uri, const std::string& pluginName = "")
    {
        auto b = std::make_shared<Box>();
        b->id         = NextBoxId();
        b->type       = BoxType::Plugin;
        b->pluginUri  = uri;
        b->pluginName = pluginName;
        b->isEnabled  = true;
        return b;
    }

    std::shared_ptr<Box> MakeMergeBox()
    {
        auto b = std::make_shared<Box>();
        b->id   = NextBoxId();
        b->type = BoxType::Merge;
        return b;
    }

    std::shared_ptr<Box> MakeAbBox()
    {
        auto b = std::make_shared<Box>();
        b->id   = NextBoxId();
        b->type = BoxType::Ab;
        return b;
    }

    std::shared_ptr<Box> MakeOutputBox(int channelIndex = 0)
    {
        auto b = std::make_shared<Box>();
        b->id                 = NextBoxId();
        b->type               = BoxType::Output;
        b->outputChannelIndex = channelIndex;
        return b;
    }

    // ---------------------------------------------------------------------------
    // Graph mutation
    // ---------------------------------------------------------------------------

    /// Add a Box to the graph. The caller should use the Make* helpers above.
    BoxId AddBox(std::shared_ptr<Box> box)
    {
        boxes_.push_back(std::move(box));
        return boxes_.back()->id;
    }

    /// Connect two Boxes. Adding the same edge twice is a no-op.
    void Connect(BoxId from, BoxId to)
    {
        for (const auto& c : connections_)
            if (c.from == from && c.to == to) return;
        connections_.emplace_back(from, to);
    }

    /// Remove a connection. Silent if it doesn't exist.
    void Disconnect(BoxId from, BoxId to)
    {
        auto it = std::remove_if(connections_.begin(), connections_.end(),
            [&](const Connection& c){ return c.from == from && c.to == to; });
        connections_.erase(it, connections_.end());
    }

    /// Remove a Box and all connections involving it.
    void RemoveBox(BoxId id)
    {
        boxes_.erase(
            std::remove_if(boxes_.begin(), boxes_.end(),
                [id](const std::shared_ptr<Box>& b){ return b->id == id; }),
            boxes_.end());
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(),
                [id](const Connection& c){ return c.from == id || c.to == id; }),
            connections_.end());
    }

    // ---------------------------------------------------------------------------
    // US-02: Traversal helpers
    // ---------------------------------------------------------------------------

    /// Find a Box by its stable ID. Returns nullptr if not found.
    Box* FindBox(BoxId id)
    {
        for (auto& b : boxes_)
            if (b->id == id) return b.get();
        return nullptr;
    }
    const Box* FindBox(BoxId id) const
    {
        for (const auto& b : boxes_)
            if (b->id == id) return b.get();
        return nullptr;
    }

    /// All plugin Boxes (type == BoxType::Plugin), in insertion order.
    std::vector<Box*> GetAllPluginBoxes()
    {
        std::vector<Box*> result;
        for (auto& b : boxes_)
            if (b->isPlugin()) result.push_back(b.get());
        return result;
    }
    std::vector<const Box*> GetAllPluginBoxes() const
    {
        std::vector<const Box*> result;
        for (const auto& b : boxes_)
            if (b->isPlugin()) result.push_back(b.get());
        return result;
    }

    /// All output Boxes.
    std::vector<Box*> GetAllOutputBoxes()
    {
        std::vector<Box*> result;
        for (auto& b : boxes_)
            if (b->isOutput()) result.push_back(b.get());
        return result;
    }
    std::vector<const Box*> GetAllOutputBoxes() const
    {
        std::vector<const Box*> result;
        for (const auto& b : boxes_)
            if (b->isOutput()) result.push_back(b.get());
        return result;
    }

    /// All Boxes in insertion order.
    std::vector<Box*> GetAllBoxes()
    {
        std::vector<Box*> result;
        result.reserve(boxes_.size());
        for (auto& b : boxes_) result.push_back(b.get());
        return result;
    }

    /// Direct downstream neighbours of a Box (boxes this one feeds into).
    std::vector<Box*> GetDownstream(BoxId id)
    {
        std::vector<Box*> result;
        for (const auto& c : connections_)
        {
            if (c.from == id)
            {
                Box* b = FindBox(c.to);
                if (b) result.push_back(b);
            }
        }
        return result;
    }

    /// Direct upstream neighbours of a Box (boxes feeding into this one).
    std::vector<Box*> GetUpstream(BoxId id)
    {
        std::vector<Box*> result;
        for (const auto& c : connections_)
        {
            if (c.to == id)
            {
                Box* b = FindBox(c.from);
                if (b) result.push_back(b);
            }
        }
        return result;
    }

    /// Number of incoming connections to a Box (fan-in count).
    int IncomingCount(BoxId id) const
    {
        int count = 0;
        for (const auto& c : connections_)
            if (c.to == id) ++count;
        return count;
    }

    /// Number of outgoing connections from a Box (fan-out count).
    int OutgoingCount(BoxId id) const
    {
        int count = 0;
        for (const auto& c : connections_)
            if (c.from == id) ++count;
        return count;
    }

    /// Compute the automatic gain compensation (in dB) for implicit fan-in.
    /// Returns 0.0 for N <= 1 (no compensation needed).
    /// Formula: -20 * log10(N)
    static float ImplicitFanInCompensationDb(int incomingCount)
    {
        if (incomingCount <= 1) return 0.0f;
        return static_cast<float>(-20.0 * std::log10(static_cast<double>(incomingCount)));
    }

    /// Walk all Boxes reachable downstream from a starting Box (DFS).
    /// Visits each Box at most once. Does not check for cycles (use
    /// IsAcyclic() first if needed).
    void WalkDownstream(BoxId startId, const std::function<void(Box*)>& visitor)
    {
        std::unordered_set<BoxId> visited;
        WalkDownstreamImpl(startId, visitor, visited);
    }

    /// Walk all Boxes reachable upstream from a starting Box (DFS).
    void WalkUpstream(BoxId startId, const std::function<void(Box*)>& visitor)
    {
        std::unordered_set<BoxId> visited;
        WalkUpstreamImpl(startId, visitor, visited);
    }

    /// Boxes with no incoming connections — graph entry points.
    std::vector<Box*> GetRootBoxes()
    {
        std::unordered_set<BoxId> hasIncoming;
        for (const auto& c : connections_)
            hasIncoming.insert(c.to);

        std::vector<Box*> roots;
        for (auto& b : boxes_)
            if (hasIncoming.find(b->id) == hasIncoming.end())
                roots.push_back(b.get());
        return roots;
    }

    /// Topological sort (Kahn's algorithm). Returns sorted Box pointers.
    /// Returns an empty vector if the graph contains a cycle.
    std::vector<Box*> TopologicalSort() const
    {
        // Build in-degree map.
        std::unordered_map<BoxId, int> inDegree;
        for (const auto& b : boxes_)
            inDegree[b->id] = 0;
        for (const auto& c : connections_)
            inDegree[c.to]++;

        std::vector<BoxId> queue;
        for (const auto& [id, deg] : inDegree)
            if (deg == 0) queue.push_back(id);

        std::vector<Box*> sorted;
        size_t head = 0;
        while (head < queue.size())
        {
            BoxId current = queue[head++];
            Box* b = const_cast<RoutingGraph*>(this)->FindBox(current);
            if (b) sorted.push_back(b);

            for (const auto& c : connections_)
            {
                if (c.from == current)
                {
                    if (--inDegree[c.to] == 0)
                        queue.push_back(c.to);
                }
            }
        }

        // If sorted doesn't contain all boxes, there's a cycle.
        if (sorted.size() != boxes_.size())
            return {};

        return sorted;
    }

    /// Returns true if the graph is acyclic (valid routing).
    bool IsAcyclic() const
    {
        return TopologicalSort().size() == boxes_.size();
    }

    /// Returns BoxIds of Boxes that are on dead branches — i.e., no
    /// OutputBox is reachable downstream from them.
    std::vector<BoxId> FindDeadBranches() const
    {
        // For each Box, check if any OutputBox is reachable downstream.
        std::vector<BoxId> dead;
        for (const auto& b : boxes_)
        {
            if (b->isOutput()) continue; // OutputBox itself is never "dead"
            bool reachesOutput = false;
            const_cast<RoutingGraph*>(this)->WalkDownstream(b->id,
                [&](Box* downstream)
                {
                    if (downstream->isOutput()) reachesOutput = true;
                });
            if (!reachesOutput)
                dead.push_back(b->id);
        }
        return dead;
    }

    // ---------------------------------------------------------------------------
    // Factory: build a default single-path graph (mirrors Pedalboard::MakeDefault)
    // ---------------------------------------------------------------------------

    static RoutingGraph MakeDefault(int numOutputChannels = 1)
    {
        RoutingGraph g;
        g.name = "Default Preset";

        // One empty plugin slot connected directly to the first output.
        auto plugin = g.MakePluginBox("uri://two-play/pipedal/pedalboard#Empty");
        auto output = g.MakeOutputBox(0);

        g.AddBox(plugin);
        g.AddBox(output);
        g.Connect(plugin->id, output->id);

        // Add remaining output boxes (not connected — available for routing).
        for (int i = 1; i < numOutputChannels; ++i)
        {
            auto out = g.MakeOutputBox(i);
            g.AddBox(out);
        }

        return g;
    }

    // ---------------------------------------------------------------------------
    // US-05: Routing validation
    // ---------------------------------------------------------------------------

    enum class ValidationSeverity
    {
        Error,    // Hard error — graph cannot be used for audio.
        Warning,  // Soft warning — graph is usable but may surprise the user.
    };

    enum class ValidationCode
    {
        // Hard errors
        cycle_detected,            // Graph contains a directed cycle.
        output_index_out_of_range, // OutputBox channel index >= available outputs.

        // Warnings
        dead_branch_warning,       // Branch does not reach any OutputBox.
        orphan_box,                // Box has no connections at all.
    };

    struct ValidationError
    {
        ValidationSeverity severity;
        ValidationCode     code;
        BoxId              boxId;     // Box involved (INVALID_BOX_ID if graph-level).
        std::string        message;   // Human-readable description.

        bool isError()   const { return severity == ValidationSeverity::Error;   }
        bool isWarning() const { return severity == ValidationSeverity::Warning; }
    };

    struct ValidationResult
    {
        std::vector<ValidationError> errors;
        std::vector<ValidationError> warnings;

        bool IsValid()       const { return errors.empty(); }
        bool HasWarnings()   const { return !warnings.empty(); }
        bool IsClean()       const { return errors.empty() && warnings.empty(); }

        void AddError(ValidationCode code, BoxId boxId, const std::string& msg)
        {
            errors.push_back({ ValidationSeverity::Error, code, boxId, msg });
        }
        void AddWarning(ValidationCode code, BoxId boxId, const std::string& msg)
        {
            warnings.push_back({ ValidationSeverity::Warning, code, boxId, msg });
        }

        // Merge another result into this one.
        void Merge(const ValidationResult& other)
        {
            errors.insert(errors.end(), other.errors.begin(), other.errors.end());
            warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());
        }
    };

    /// Validate the routing graph.
    ///
    /// @param availableOutputChannels  Number of physical output channels on the
    ///                                 connected audio interface. Pass -1 to skip
    ///                                 output channel range checks (e.g. no
    ///                                 interface connected yet).
    ///
    /// Rules applied:
    ///   ERROR   cycle_detected            — graph contains a directed cycle
    ///   ERROR   output_index_out_of_range — OutputBox.channelIndex >= availableOutputChannels
    ///   WARNING dead_branch_warning       — box has no path to any OutputBox
    ///   WARNING orphan_box                — box has no connections at all
    ValidationResult Validate(int availableOutputChannels = -1) const
    {
        ValidationResult result;

        // ------------------------------------------------------------------
        // 1. Cycle detection (hard error — all other checks are meaningless
        //    if the graph has a cycle, so return early).
        // ------------------------------------------------------------------
        if (!IsAcyclic())
        {
            result.AddError(
                ValidationCode::cycle_detected,
                INVALID_BOX_ID,
                "Routing graph contains a cycle. Signal flow must be directed and acyclic.");
            return result; // remaining checks require a DAG
        }

        // ------------------------------------------------------------------
        // 2. Output channel index range check (hard error per OutputBox).
        // ------------------------------------------------------------------
        if (availableOutputChannels >= 0)
        {
            for (const auto& b : boxes_)
            {
                if (b->isOutput() && b->outputChannelIndex >= availableOutputChannels)
                {
                    result.AddError(
                        ValidationCode::output_index_out_of_range,
                        b->id,
                        "OutputBox channel index " +
                            std::to_string(b->outputChannelIndex) +
                            " is out of range (interface has " +
                            std::to_string(availableOutputChannels) +
                            " output channel(s)).");
                }
            }
        }

        // ------------------------------------------------------------------
        // 3. Orphan boxes — no connections at all (warning).
        // ------------------------------------------------------------------
        for (const auto& b : boxes_)
        {
            if (IncomingCount(b->id) == 0 && OutgoingCount(b->id) == 0)
            {
                result.AddWarning(
                    ValidationCode::orphan_box,
                    b->id,
                    "Box '" + (b->title.empty() ? b->pluginUri : b->title) +
                        "' has no connections.");
            }
        }

        // ------------------------------------------------------------------
        // 4. Dead branch detection (warning).
        //    Skip boxes that are already flagged as orphans to avoid
        //    duplicate warnings.
        // ------------------------------------------------------------------
        std::unordered_set<BoxId> orphanIds;
        for (const auto& w : result.warnings)
            if (w.code == ValidationCode::orphan_box)
                orphanIds.insert(w.boxId);

        auto deadIds = FindDeadBranches();
        for (BoxId deadId : deadIds)
        {
            if (orphanIds.count(deadId)) continue; // already warned
            const Box* b = FindBox(deadId);
            std::string label = b ? (b->title.empty() ? b->pluginUri : b->title) : std::to_string(deadId);
            result.AddWarning(
                ValidationCode::dead_branch_warning,
                deadId,
                "Box '" + label + "' is on a dead branch — no OutputBox is reachable downstream.");
        }

        return result;
    }

    void write_json(json_writer& writer) const override;
    void read_json(json_reader& reader) override;

private:
    void WalkDownstreamImpl(BoxId id,
                            const std::function<void(Box*)>& visitor,
                            std::unordered_set<BoxId>& visited)
    {
        if (visited.count(id)) return;
        visited.insert(id);
        Box* b = FindBox(id);
        if (b) visitor(b);
        for (const auto& c : connections_)
            if (c.from == id)
                WalkDownstreamImpl(c.to, visitor, visited);
    }

    void WalkUpstreamImpl(BoxId id,
                          const std::function<void(Box*)>& visitor,
                          std::unordered_set<BoxId>& visited)
    {
        if (visited.count(id)) return;
        visited.insert(id);
        Box* b = FindBox(id);
        if (b) visitor(b);
        for (const auto& c : connections_)
            if (c.to == id)
                WalkUpstreamImpl(c.from, visitor, visited);
    }
};

} // namespace pipedal
