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
// PedalboardRoutingTest.cpp
//
// Tests for US-03 (RoutingGraph embedded in Pedalboard, lazy rebuild from
// items_) and US-04 (traversal utilities delegating to the new model).
//
// Tags: [Build][pedalboard_routing]
// ---------------------------------------------------------------------------

#include "pch.h"
#include <catch/catch.hpp>
#include "Pedalboard.hpp"
#include "RoutingGraph.hpp"

using namespace pipedal;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static PedalboardItem MakeTestItem(int64_t id, const std::string& uri)
{
    PedalboardItem item;
    item.instanceId(id);
    item.uri(uri);
    item.isEnabled(true);
    return item;
}

// ---------------------------------------------------------------------------
// Suite 1 — RoutingGraph is embedded in Pedalboard and accessible
// ---------------------------------------------------------------------------

TEST_CASE("Pedalboard – GetRoutingGraph returns a valid graph for default pedalboard",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb = Pedalboard::MakeDefault();
    const RoutingGraph& g = pb.GetRoutingGraph();

    // Default pedalboard has one empty item connected to output.
    REQUIRE(g.IsAcyclic());
    REQUIRE_FALSE(g.GetAllBoxes().empty());
    REQUIRE_FALSE(g.GetAllOutputBoxes().empty());
}

TEST_CASE("Pedalboard – default graph has no dead branches",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb = Pedalboard::MakeDefault();
    const RoutingGraph& g = pb.GetRoutingGraph();
    REQUIRE(g.FindDeadBranches().empty());
}

TEST_CASE("Pedalboard – routing graph is rebuilt after MarkRoutingGraphDirty",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb = Pedalboard::MakeDefault();

    // Access once to trigger initial build.
    size_t initialBoxCount = pb.GetRoutingGraph().GetAllBoxes().size();

    // Add a new item to items_ and mark dirty.
    pb.items().push_back(MakeTestItem(999, "lv2:TestPlugin"));
    pb.MarkRoutingGraphDirty();

    // Graph should now reflect the new item.
    size_t newBoxCount = pb.GetRoutingGraph().GetAllBoxes().size();
    REQUIRE(newBoxCount > initialBoxCount);
}

TEST_CASE("Pedalboard – SyncRoutingGraph forces immediate rebuild",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb = Pedalboard::MakeDefault();
    pb.items().push_back(MakeTestItem(998, "lv2:AnotherPlugin"));
    pb.SyncRoutingGraph();

    // Graph must include the new item without waiting for lazy access.
    const RoutingGraph& g = pb.GetRoutingGraph();
    bool found = false;
    for (const auto* box : g.GetAllBoxes())
    {
        if (box->pluginUri == "lv2:AnotherPlugin") { found = true; break; }
    }
    REQUIRE(found);
}

// ---------------------------------------------------------------------------
// Suite 2 — Legacy items_ → RoutingGraph conversion correctness
// ---------------------------------------------------------------------------

TEST_CASE("Pedalboard – each PedalboardItem instanceId maps to a BoxId in the graph",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb = Pedalboard::MakeDefault();
    // The default item has some instanceId assigned by MakeEmptyItem.
    auto allItems = pb.GetAllPlugins();
    REQUIRE_FALSE(allItems.empty());

    const RoutingGraph& g = pb.GetRoutingGraph();
    for (const auto* item : allItems)
    {
        // Every PedalboardItem should be findable by its instanceId in the graph.
        const Box* box = g.FindBox(item->instanceId());
        REQUIRE(box != nullptr);
        REQUIRE(box->pluginUri == item->uri());
    }
}

TEST_CASE("Pedalboard – serial chain maps to sequential connections in graph",
          "[Build][pedalboard_routing]")
{
    // Build a 3-item serial chain manually.
    Pedalboard pb;
    pb.name("test");
    pb.items().push_back(MakeTestItem(1, "lv2:A"));
    pb.items().push_back(MakeTestItem(2, "lv2:B"));
    pb.items().push_back(MakeTestItem(3, "lv2:C"));
    pb.SyncRoutingGraph();

    const RoutingGraph& g = pb.GetRoutingGraph();

    // A → B → C → OutputBox
    // Each box except the last has exactly one outgoing connection.
    REQUIRE(g.OutgoingCount(1) == 1);
    REQUIRE(g.OutgoingCount(2) == 1);
    REQUIRE(g.OutgoingCount(3) == 1); // connects to output box

    // B is downstream of A.
    auto downA = g.GetDownstream(1);
    REQUIRE(downA.size() == 1);
    REQUIRE(downA[0]->id == 2);

    REQUIRE(g.IsAcyclic());
}

TEST_CASE("Pedalboard – split item maps to fan-out in graph",
          "[Build][pedalboard_routing]")
{
    // Build: [plugin] → [split → top:[item] / bottom:[item]]
    Pedalboard pb;
    pb.name("test");

    PedalboardItem plugin = MakeTestItem(1, "lv2:Plugin");
    pb.items().push_back(plugin);

    PedalboardItem split;
    split.instanceId(10);
    split.uri(SPLIT_PEDALBOARD_ITEM_URI);
    split.isEnabled(true);
    split.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY, 0.0f));
    split.topChain().push_back(MakeTestItem(11, EMPTY_PEDALBOARD_ITEM_URI));
    split.bottomChain().push_back(MakeTestItem(12, EMPTY_PEDALBOARD_ITEM_URI));
    pb.items().push_back(split);

    pb.SyncRoutingGraph();
    const RoutingGraph& g = pb.GetRoutingGraph();

    // Split box (id=10) should have 2 outgoing connections (fan-out to top and bottom).
    REQUIRE(g.OutgoingCount(10) == 2);

    // Top and bottom leaf boxes should be reachable from split.
    auto downSplit = g.GetDownstream(10);
    REQUIRE(downSplit.size() == 2);

    REQUIRE(g.IsAcyclic());
}

TEST_CASE("Pedalboard – split item with Ab splitType maps to BoxType::Ab",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb;
    pb.name("test");

    PedalboardItem split;
    split.instanceId(10);
    split.uri(SPLIT_PEDALBOARD_ITEM_URI);
    split.isEnabled(true);
    split.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY, 0.0f)); // 0 = A/B
    split.topChain().push_back(MakeTestItem(11, EMPTY_PEDALBOARD_ITEM_URI));
    split.bottomChain().push_back(MakeTestItem(12, EMPTY_PEDALBOARD_ITEM_URI));
    pb.items().push_back(split);
    pb.SyncRoutingGraph();

    const Box* box = pb.GetRoutingGraph().FindBox(10);
    REQUIRE(box != nullptr);
    REQUIRE(box->isAb());
}

TEST_CASE("Pedalboard – split item with Mix splitType maps to BoxType::Merge",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb;
    pb.name("test");

    PedalboardItem split;
    split.instanceId(10);
    split.uri(SPLIT_PEDALBOARD_ITEM_URI);
    split.isEnabled(true);
    split.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY, 1.0f)); // 1 = Mix
    split.topChain().push_back(MakeTestItem(11, EMPTY_PEDALBOARD_ITEM_URI));
    split.bottomChain().push_back(MakeTestItem(12, EMPTY_PEDALBOARD_ITEM_URI));
    pb.items().push_back(split);
    pb.SyncRoutingGraph();

    const Box* box = pb.GetRoutingGraph().FindBox(10);
    REQUIRE(box != nullptr);
    REQUIRE(box->isMerge());
}

// ---------------------------------------------------------------------------
// Suite 3 — DeepCopy produces an independent graph
// ---------------------------------------------------------------------------

TEST_CASE("Pedalboard – DeepCopy produces an independent routing graph",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb = Pedalboard::MakeDefault();
    pb.GetRoutingGraph(); // trigger initial build

    Pedalboard copy = pb.DeepCopy();

    // Add an item to the copy and rebuild.
    copy.items().push_back(MakeTestItem(500, "lv2:CopyOnly"));
    copy.SyncRoutingGraph();

    // Original should not see the new item.
    REQUIRE(pb.GetRoutingGraph().FindBox(500) == nullptr);

    // Copy should see it.
    REQUIRE(copy.GetRoutingGraph().FindBox(500) != nullptr);
}

// ---------------------------------------------------------------------------
// Suite 4 — GetAllPlugins and GetItem still work (US-04 regression)
// ---------------------------------------------------------------------------

TEST_CASE("Pedalboard – GetAllPlugins returns all items including split chains",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb;
    pb.name("test");
    pb.items().push_back(MakeTestItem(1, "lv2:A"));

    PedalboardItem split;
    split.instanceId(10);
    split.uri(SPLIT_PEDALBOARD_ITEM_URI);
    split.isEnabled(true);
    split.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY, 0.0f));
    split.topChain().push_back(MakeTestItem(11, "lv2:Top"));
    split.bottomChain().push_back(MakeTestItem(12, "lv2:Bottom"));
    pb.items().push_back(split);

    auto all = pb.GetAllPlugins();

    // Should contain: A(1), split(10), top(11), bottom(12)
    REQUIRE(all.size() == 4);

    std::vector<int64_t> ids;
    for (auto* item : all) ids.push_back(item->instanceId());
    REQUIRE(std::find(ids.begin(), ids.end(), 1)  != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), 10) != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), 11) != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), 12) != ids.end());
}

TEST_CASE("Pedalboard – GetItem finds items inside split chains",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb;
    pb.name("test");

    PedalboardItem split;
    split.instanceId(10);
    split.uri(SPLIT_PEDALBOARD_ITEM_URI);
    split.isEnabled(true);
    split.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY, 0.0f));
    split.topChain().push_back(MakeTestItem(11, "lv2:Top"));
    split.bottomChain().push_back(MakeTestItem(12, "lv2:Bottom"));
    pb.items().push_back(split);

    REQUIRE(pb.GetItem(10) != nullptr);
    REQUIRE(pb.GetItem(11) != nullptr);
    REQUIRE(pb.GetItem(12) != nullptr);
    REQUIRE(pb.GetItem(99) == nullptr);
}

TEST_CASE("Pedalboard – GetItem and RoutingGraph FindBox agree on presence",
          "[Build][pedalboard_routing]")
{
    Pedalboard pb = Pedalboard::MakeDefault();
    auto allItems = pb.GetAllPlugins();

    for (const auto* item : allItems)
    {
        // Both lookup paths must agree.
        bool inItems = (pb.GetItem(item->instanceId()) != nullptr);
        bool inGraph = (pb.GetRoutingGraph().FindBox(item->instanceId()) != nullptr);
        REQUIRE(inItems == inGraph);
    }

    // Non-existent id must be absent in both.
    REQUIRE(pb.GetItem(99999) == nullptr);
    REQUIRE(pb.GetRoutingGraph().FindBox(99999) == nullptr);
}
