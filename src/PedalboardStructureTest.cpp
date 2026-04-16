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
// PedalboardStructureTest.cpp
//
// Characterization tests for the recursive split model implemented in
// PedalboardItem::IsStructurallyIdentical and Pedalboard::IsStructureIdentical.
//
// These tests pin the *current* observable behavior so that future migrations
// toward the routing-tree architecture can be performed with confidence that
// no silent regressions are introduced.
//
// Test philosophy:
//   - Use only public helpers (MakeDefault / MakeSplit / MakeEmptyItem) and
//     direct field setters to construct fixtures.  No mocking of PluginHost.
//   - Every test is named for the scenario it characterizes, not for the
//     implementation detail it touches.
//   - Tags:  [Build]  – always run in CI
//            [pedalboard_structure] – run this suite in isolation
// ---------------------------------------------------------------------------

#include "pch.h"
#include <catch/catch.hpp>
#include "Pedalboard.hpp"

using namespace pipedal;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a minimal non-split PedalboardItem with the given instanceId and uri.
static PedalboardItem MakeLeaf(int64_t instanceId, const std::string& uri)
{
    PedalboardItem item;
    item.instanceId(instanceId);
    item.uri(uri);
    item.isEnabled(true);
    return item;
}

/// Build a split item whose topChain and bottomChain each contain one leaf.
/// controlValues are populated to satisfy the splitType lookup performed by
/// IsStructurallyIdentical.
static PedalboardItem MakeSplitItem(
    int64_t splitInstanceId,
    int64_t topLeafId,
    int64_t bottomLeafId,
    float splitType = 0.0f,
    const std::string& leafUri = "uri://two-play/pipedal/pedalboard#Empty")
{
    PedalboardItem split;
    split.instanceId(splitInstanceId);
    split.uri(SPLIT_PEDALBOARD_ITEM_URI);
    split.isEnabled(true);
    split.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY, splitType));
    split.controlValues().push_back(ControlValue(SPLIT_SELECT_KEY,    0.0f));
    split.controlValues().push_back(ControlValue(SPLIT_MIX_KEY,       0.0f));
    split.controlValues().push_back(ControlValue(SPLIT_PANL_KEY,      0.0f));
    split.controlValues().push_back(ControlValue(SPLIT_VOLL_KEY,      -3.0f));
    split.controlValues().push_back(ControlValue(SPLIT_PANR_KEY,      0.0f));
    split.controlValues().push_back(ControlValue(SPLIT_VOLR_KEY,      -3.0f));

    split.topChain().push_back(MakeLeaf(topLeafId,    leafUri));
    split.bottomChain().push_back(MakeLeaf(bottomLeafId, leafUri));
    return split;
}

// ---------------------------------------------------------------------------
// Suite 1 – flat (non-split) items
// ---------------------------------------------------------------------------

TEST_CASE("IsStructurallyIdentical – identical flat items are identical",
          "[Build][pedalboard_structure]")
{
    auto a = MakeLeaf(1, "lv2:SomePlugin");
    auto b = MakeLeaf(1, "lv2:SomePlugin");
    REQUIRE(a.IsStructurallyIdentical(b));
}

TEST_CASE("IsStructurallyIdentical – different instanceId makes items non-identical",
          "[Build][pedalboard_structure]")
{
    auto a = MakeLeaf(1, "lv2:SomePlugin");
    auto b = MakeLeaf(2, "lv2:SomePlugin");
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

TEST_CASE("IsStructurallyIdentical – different uri makes items non-identical",
          "[Build][pedalboard_structure]")
{
    auto a = MakeLeaf(1, "lv2:PluginA");
    auto b = MakeLeaf(1, "lv2:PluginB");
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

// ---------------------------------------------------------------------------
// Suite 2 – split items: structural checks
// ---------------------------------------------------------------------------

TEST_CASE("IsStructurallyIdentical – identical split items are identical",
          "[Build][pedalboard_structure]")
{
    auto a = MakeSplitItem(10, 11, 12);
    auto b = MakeSplitItem(10, 11, 12);
    REQUIRE(a.IsStructurallyIdentical(b));
}

TEST_CASE("IsStructurallyIdentical – different splitType means non-identical",
          "[Build][pedalboard_structure]")
{
    // splitType differences can trigger buffer allocation changes in the DSP
    // layer, so the original code treats them as structural.
    auto a = MakeSplitItem(10, 11, 12, /*splitType=*/0.0f);
    auto b = MakeSplitItem(10, 11, 12, /*splitType=*/1.0f);
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

// ---------------------------------------------------------------------------
// Suite 3 – topChain comparisons
// ---------------------------------------------------------------------------

TEST_CASE("IsStructurallyIdentical – different topChain size means non-identical",
          "[Build][pedalboard_structure]")
{
    auto a = MakeSplitItem(10, 11, 12);
    auto b = MakeSplitItem(10, 11, 12);
    // Push an extra leaf into b's topChain.
    b.topChain().push_back(MakeLeaf(99, EMPTY_PEDALBOARD_ITEM_URI));
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

TEST_CASE("IsStructurallyIdentical – different topChain leaf instanceId means non-identical",
          "[Build][pedalboard_structure]")
{
    auto a = MakeSplitItem(10, 11, 12);
    auto b = MakeSplitItem(10, /*topLeafId=*/99, 12); // different top leaf id
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

TEST_CASE("IsStructurallyIdentical – different topChain leaf uri means non-identical",
          "[Build][pedalboard_structure]")
{
    auto a = MakeSplitItem(10, 11, 12);
    auto b = MakeSplitItem(10, 11, 12);
    b.topChain()[0].uri("lv2:DifferentPlugin");
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

// ---------------------------------------------------------------------------
// Suite 4 – bottomChain comparisons  (these tests characterize the bug fix)
// ---------------------------------------------------------------------------

TEST_CASE("IsStructurallyIdentical – different bottomChain size means non-identical",
          "[Build][pedalboard_structure]")
{
    // PRE-FIX BUG: the size-mismatch guard was inverted, so unequal bottomChain
    // sizes caused an out-of-bounds item loop instead of an immediate false.
    // This test would have exhibited undefined behaviour or a wrong result before
    // the fix.
    auto a = MakeSplitItem(10, 11, 12);
    auto b = MakeSplitItem(10, 11, 12);
    b.bottomChain().push_back(MakeLeaf(99, EMPTY_PEDALBOARD_ITEM_URI));
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

TEST_CASE("IsStructurallyIdentical – different bottomChain leaf instanceId means non-identical",
          "[Build][pedalboard_structure]")
{
    // PRE-FIX BUG: when bottomChain sizes were equal the item loop was SKIPPED,
    // so this case would have incorrectly returned true before the fix.
    auto a = MakeSplitItem(10, 11, 12);
    auto b = MakeSplitItem(10, 11, /*bottomLeafId=*/99); // different bottom leaf id
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

TEST_CASE("IsStructurallyIdentical – different bottomChain leaf uri means non-identical",
          "[Build][pedalboard_structure]")
{
    // PRE-FIX BUG: same as above – equal-size chains were never recursed into.
    auto a = MakeSplitItem(10, 11, 12);
    auto b = MakeSplitItem(10, 11, 12);
    b.bottomChain()[0].uri("lv2:DifferentPlugin");
    REQUIRE_FALSE(a.IsStructurallyIdentical(b));
}

TEST_CASE("IsStructurallyIdentical – identical bottomChain with multiple items is identical",
          "[Build][pedalboard_structure]")
{
    auto a = MakeSplitItem(10, 11, 12);
    a.bottomChain().push_back(MakeLeaf(13, "lv2:AnotherPlugin"));

    auto b = MakeSplitItem(10, 11, 12);
    b.bottomChain().push_back(MakeLeaf(13, "lv2:AnotherPlugin"));

    REQUIRE(a.IsStructurallyIdentical(b));
}

// ---------------------------------------------------------------------------
// Suite 5 – deeply nested (recursive) splits
// ---------------------------------------------------------------------------

TEST_CASE("IsStructurallyIdentical – identical nested splits are identical",
          "[Build][pedalboard_structure]")
{
    // Outer split: instanceId 10, top→20, bottom is itself a split (30→31,32)
    auto inner = MakeSplitItem(30, 31, 32);

    PedalboardItem outerA;
    outerA.instanceId(10);
    outerA.uri(SPLIT_PEDALBOARD_ITEM_URI);
    outerA.isEnabled(true);
    outerA.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY, 0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_SELECT_KEY,    0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_MIX_KEY,       0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_PANL_KEY,      0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_VOLL_KEY,      -3.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_PANR_KEY,      0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_VOLR_KEY,      -3.0f));
    outerA.topChain().push_back(MakeLeaf(20, EMPTY_PEDALBOARD_ITEM_URI));
    outerA.bottomChain().push_back(inner);

    // Deep-copy for outerB (struct copy is value semantics in this model).
    PedalboardItem outerB = outerA;

    REQUIRE(outerA.IsStructurallyIdentical(outerB));
}

TEST_CASE("IsStructurallyIdentical – nested split bottomChain leaf change detected",
          "[Build][pedalboard_structure]")
{
    auto innerA = MakeSplitItem(30, 31, 32);
    auto innerB = MakeSplitItem(30, 31, /*bottomLeafId=*/99); // differs deep inside

    PedalboardItem outerA;
    outerA.instanceId(10);
    outerA.uri(SPLIT_PEDALBOARD_ITEM_URI);
    outerA.isEnabled(true);
    outerA.controlValues().push_back(ControlValue(SPLIT_SPLITTYPE_KEY, 0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_SELECT_KEY,    0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_MIX_KEY,       0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_PANL_KEY,      0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_VOLL_KEY,      -3.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_PANR_KEY,      0.0f));
    outerA.controlValues().push_back(ControlValue(SPLIT_VOLR_KEY,      -3.0f));
    outerA.topChain().push_back(MakeLeaf(20, EMPTY_PEDALBOARD_ITEM_URI));
    outerA.bottomChain().push_back(innerA);

    PedalboardItem outerB = outerA;               // value-copy the base
    outerB.bottomChain()[0] = innerB;             // replace inner with divergent

    // The recursive comparison must propagate down and detect the leaf difference.
    REQUIRE_FALSE(outerA.IsStructurallyIdentical(outerB));
}

// ---------------------------------------------------------------------------
// Suite 6 – Pedalboard::IsStructureIdentical (top-level convenience method)
// ---------------------------------------------------------------------------

TEST_CASE("IsStructureIdentical – default pedalboard is identical to itself",
          "[Build][pedalboard_structure]")
{
    Pedalboard a = Pedalboard::MakeDefault();
    Pedalboard b = a; // value copy
    REQUIRE(a.IsStructureIdentical(b));
}

TEST_CASE("IsStructureIdentical – pedalboard with a split is identical to its copy",
          "[Build][pedalboard_structure]")
{
    Pedalboard a = Pedalboard::MakeDefault();
    a.items().push_back(a.MakeSplit());
    Pedalboard b = a;
    REQUIRE(a.IsStructureIdentical(b));
}

TEST_CASE("IsStructureIdentical – adding an item to bottomChain makes boards non-identical",
          "[Build][pedalboard_structure]")
{
    Pedalboard a = Pedalboard::MakeDefault();
    a.items().push_back(a.MakeSplit());

    Pedalboard b = a;
    // Mutate b's split item bottomChain in place.
    for (auto& item : b.items())
    {
        if (item.isSplit())
        {
            item.bottomChain().push_back(b.MakeEmptyItem());
            break;
        }
    }
    REQUIRE_FALSE(a.IsStructureIdentical(b));
}

TEST_CASE("IsStructureIdentical – changing bottomChain leaf instanceId makes boards non-identical",
          "[Build][pedalboard_structure]")
{
    // Regression test: before the fix, equal-size bottomChains were NEVER compared,
    // so this would have returned true (a false identical).
    Pedalboard a = Pedalboard::MakeDefault();
    a.items().push_back(a.MakeSplit());

    Pedalboard b = a;
    for (auto& item : b.items())
    {
        if (item.isSplit() && !item.bottomChain().empty())
        {
            // Corrupt the leaf instance id so it no longer matches a.
            item.bottomChain()[0].instanceId(item.bottomChain()[0].instanceId() + 1000);
            break;
        }
    }
    REQUIRE_FALSE(a.IsStructureIdentical(b));
}

TEST_CASE("IsStructureIdentical – nextInstanceId mismatch short-circuits comparison",
          "[Build][pedalboard_structure]")
{
    Pedalboard a = Pedalboard::MakeDefault();
    // Make a different board: MakeSplit increments nextInstanceId internally.
    Pedalboard b = Pedalboard::MakeDefault();
    b.items().push_back(b.MakeSplit());
    // The nextInstanceId counters now differ, so the quick-check at the top of
    // IsStructureIdentical must detect this before doing any item recursion.
    REQUIRE_FALSE(a.IsStructureIdentical(b));
}
