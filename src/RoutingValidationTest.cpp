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
// RoutingValidationTest.cpp
//
// Tests for US-05 — routing validation rules.
//
// Covers:
//   ERROR   cycle_detected
//   ERROR   output_index_out_of_range
//   WARNING dead_branch_warning
//   WARNING orphan_box
//
// Tags: [Build][routing_validation]
// ---------------------------------------------------------------------------

#include "pch.h"
#include <catch/catch.hpp>
#include "RoutingGraph.hpp"
#include "Pedalboard.hpp"

using namespace pipedal;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool HasError(const RoutingGraph::ValidationResult& r, RoutingGraph::ValidationCode code)
{
    for (const auto& e : r.errors)
        if (e.code == code) return true;
    return false;
}

static bool HasWarning(const RoutingGraph::ValidationResult& r, RoutingGraph::ValidationCode code)
{
    for (const auto& w : r.warnings)
        if (w.code == code) return true;
    return false;
}

static bool HasErrorForBox(const RoutingGraph::ValidationResult& r,
                           RoutingGraph::ValidationCode code, BoxId id)
{
    for (const auto& e : r.errors)
        if (e.code == code && e.boxId == id) return true;
    return false;
}

static bool HasWarningForBox(const RoutingGraph::ValidationResult& r,
                             RoutingGraph::ValidationCode code, BoxId id)
{
    for (const auto& w : r.warnings)
        if (w.code == code && w.boxId == id) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Suite 1 — Clean graphs
// ---------------------------------------------------------------------------

TEST_CASE("Validation – fully connected serial chain is clean",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto o = g.MakeOutputBox(0);       g.AddBox(o);
    g.Connect(a->id, o->id);

    auto r = g.Validate(2);
    REQUIRE(r.IsClean());
}

TEST_CASE("Validation – default pedalboard is clean",
          "[Build][routing_validation]")
{
    auto g = RoutingGraph::MakeDefault(1);
    REQUIRE(g.Validate(2).IsClean());
}

TEST_CASE("Validation – two-amp two-output graph is clean",
          "[Build][routing_validation]")
{
    // The motivating Scarlett 2i2 example.
    RoutingGraph g;
    auto amp1 = g.MakePluginBox("lv2:Amp1"); g.AddBox(amp1);
    auto ir1  = g.MakePluginBox("lv2:IR1");  g.AddBox(ir1);
    auto amp2 = g.MakePluginBox("lv2:Amp2"); g.AddBox(amp2);
    auto ir2  = g.MakePluginBox("lv2:IR2");  g.AddBox(ir2);
    auto out1 = g.MakeOutputBox(0);          g.AddBox(out1);
    auto out2 = g.MakeOutputBox(1);          g.AddBox(out2);

    g.Connect(amp1->id, ir1->id);
    g.Connect(ir1->id,  out1->id);
    g.Connect(amp2->id, ir2->id);
    g.Connect(ir2->id,  out2->id);

    auto r = g.Validate(2);
    REQUIRE(r.IsClean());
}

TEST_CASE("Validation – fan-out then implicit fan-in is clean",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto amp    = g.MakePluginBox("lv2:Amp");    g.AddBox(amp);
    auto delay  = g.MakePluginBox("lv2:Delay");  g.AddBox(delay);
    auto chorus = g.MakePluginBox("lv2:Chorus"); g.AddBox(chorus);
    auto cab    = g.MakePluginBox("lv2:Cab");    g.AddBox(cab);
    auto out    = g.MakeOutputBox(0);            g.AddBox(out);

    g.Connect(amp->id,    delay->id);
    g.Connect(amp->id,    chorus->id);
    g.Connect(delay->id,  cab->id);
    g.Connect(chorus->id, cab->id);
    g.Connect(cab->id,    out->id);

    REQUIRE(g.Validate(2).IsClean());
}

// ---------------------------------------------------------------------------
// Suite 2 — cycle_detected (hard error)
// ---------------------------------------------------------------------------

TEST_CASE("Validation – direct cycle produces cycle_detected error",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);
    g.Connect(a->id, b->id);
    g.Connect(b->id, a->id);

    auto r = g.Validate();
    REQUIRE_FALSE(r.IsValid());
    REQUIRE(HasError(r, RoutingGraph::ValidationCode::cycle_detected));
}

TEST_CASE("Validation – self-loop produces cycle_detected error",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    g.Connect(a->id, a->id);

    auto r = g.Validate();
    REQUIRE(HasError(r, RoutingGraph::ValidationCode::cycle_detected));
}

TEST_CASE("Validation – cycle_detected suppresses other checks",
          "[Build][routing_validation]")
{
    // When there is a cycle, dead branch / orphan checks are skipped
    // because topological traversal would be infinite.
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);
    g.Connect(a->id, b->id);
    g.Connect(b->id, a->id);

    auto r = g.Validate();
    REQUIRE(HasError(r, RoutingGraph::ValidationCode::cycle_detected));
    // No dead branch or orphan warnings — those require an acyclic graph.
    REQUIRE_FALSE(HasWarning(r, RoutingGraph::ValidationCode::dead_branch_warning));
    REQUIRE_FALSE(HasWarning(r, RoutingGraph::ValidationCode::orphan_box));
}

// ---------------------------------------------------------------------------
// Suite 3 — output_index_out_of_range (hard error)
// ---------------------------------------------------------------------------

TEST_CASE("Validation – OutputBox in range is not an error",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto o = g.MakeOutputBox(0);       g.AddBox(o); // channel 0, interface has 2
    g.Connect(a->id, o->id);

    auto r = g.Validate(2);
    REQUIRE_FALSE(HasError(r, RoutingGraph::ValidationCode::output_index_out_of_range));
}

TEST_CASE("Validation – OutputBox out of range produces error",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto o = g.MakeOutputBox(2);       g.AddBox(o); // channel 2, but interface has only 2 (0,1)
    g.Connect(a->id, o->id);

    auto r = g.Validate(2);
    REQUIRE(HasErrorForBox(r, RoutingGraph::ValidationCode::output_index_out_of_range, o->id));
}

TEST_CASE("Validation – output range check skipped when availableOutputChannels is -1",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto o = g.MakeOutputBox(99);      g.AddBox(o); // absurd index — ignored when -1
    g.Connect(a->id, o->id);

    auto r = g.Validate(-1); // -1 = no interface connected
    REQUIRE_FALSE(HasError(r, RoutingGraph::ValidationCode::output_index_out_of_range));
}

TEST_CASE("Validation – each out-of-range OutputBox gets its own error",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a  = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto o1 = g.MakeOutputBox(5);       g.AddBox(o1);
    auto o2 = g.MakeOutputBox(6);       g.AddBox(o2);
    g.Connect(a->id, o1->id);
    g.Connect(a->id, o2->id);

    auto r = g.Validate(2);
    REQUIRE(HasErrorForBox(r, RoutingGraph::ValidationCode::output_index_out_of_range, o1->id));
    REQUIRE(HasErrorForBox(r, RoutingGraph::ValidationCode::output_index_out_of_range, o2->id));
}

// ---------------------------------------------------------------------------
// Suite 4 — dead_branch_warning
// ---------------------------------------------------------------------------

TEST_CASE("Validation – disconnected plugin box produces dead_branch_warning",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a    = g.MakePluginBox("lv2:A"); g.AddBox(a); // connected
    auto dead = g.MakePluginBox("lv2:Dead"); g.AddBox(dead); // dead — only incoming, no path to output
    auto o    = g.MakeOutputBox(0);       g.AddBox(o);

    g.Connect(a->id, o->id);
    // dead has no connections at all → orphan, not dead_branch
    // Add an incoming connection to make it a true dead branch (has input, no output path)
    g.Connect(a->id, dead->id);

    auto r = g.Validate(2);
    REQUIRE(HasWarningForBox(r, RoutingGraph::ValidationCode::dead_branch_warning, dead->id));
}

TEST_CASE("Validation – branch reaching output indirectly is not dead",
          "[Build][routing_validation]")
{
    // chorus reaches output via cab
    RoutingGraph g;
    auto amp    = g.MakePluginBox("lv2:Amp");    g.AddBox(amp);
    auto delay  = g.MakePluginBox("lv2:Delay");  g.AddBox(delay);
    auto chorus = g.MakePluginBox("lv2:Chorus"); g.AddBox(chorus);
    auto cab    = g.MakePluginBox("lv2:Cab");    g.AddBox(cab);
    auto out    = g.MakeOutputBox(0);            g.AddBox(out);

    g.Connect(amp->id,    delay->id);
    g.Connect(amp->id,    chorus->id);
    g.Connect(delay->id,  cab->id);
    g.Connect(chorus->id, cab->id);
    g.Connect(cab->id,    out->id);

    auto r = g.Validate(2);
    REQUIRE_FALSE(HasWarning(r, RoutingGraph::ValidationCode::dead_branch_warning));
}

TEST_CASE("Validation – dead branch is a warning not an error",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a    = g.MakePluginBox("lv2:A");    g.AddBox(a);
    auto dead = g.MakePluginBox("lv2:Dead"); g.AddBox(dead);
    auto o    = g.MakeOutputBox(0);          g.AddBox(o);

    g.Connect(a->id,    o->id);
    g.Connect(a->id, dead->id); // dead has no onward path

    auto r = g.Validate(2);
    REQUIRE(r.IsValid());       // no hard errors
    REQUIRE(r.HasWarnings());   // but there are warnings
    REQUIRE(HasWarning(r, RoutingGraph::ValidationCode::dead_branch_warning));
}

// ---------------------------------------------------------------------------
// Suite 5 — orphan_box
// ---------------------------------------------------------------------------

TEST_CASE("Validation – box with no connections at all is an orphan",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a       = g.MakePluginBox("lv2:A");      g.AddBox(a);
    auto orphan  = g.MakePluginBox("lv2:Orphan"); g.AddBox(orphan); // no connections
    auto o       = g.MakeOutputBox(0);            g.AddBox(o);

    g.Connect(a->id, o->id);

    auto r = g.Validate(2);
    REQUIRE(HasWarningForBox(r, RoutingGraph::ValidationCode::orphan_box, orphan->id));
}

TEST_CASE("Validation – orphan is a warning not an error",
          "[Build][routing_validation]")
{
    RoutingGraph g;
    auto a      = g.MakePluginBox("lv2:A");      g.AddBox(a);
    auto orphan = g.MakePluginBox("lv2:Orphan"); g.AddBox(orphan);
    auto o      = g.MakeOutputBox(0);            g.AddBox(o);

    g.Connect(a->id, o->id);

    auto r = g.Validate(2);
    REQUIRE(r.IsValid());     // no hard errors
    REQUIRE(r.HasWarnings()); // but there's a warning
}

TEST_CASE("Validation – orphan box is not also flagged as dead branch",
          "[Build][routing_validation]")
{
    // An orphan is a special case of dead branch — we should not emit both warnings.
    RoutingGraph g;
    auto a      = g.MakePluginBox("lv2:A");      g.AddBox(a);
    auto orphan = g.MakePluginBox("lv2:Orphan"); g.AddBox(orphan);
    auto o      = g.MakeOutputBox(0);            g.AddBox(o);

    g.Connect(a->id, o->id);

    auto r = g.Validate(2);
    REQUIRE(HasWarningForBox(r, RoutingGraph::ValidationCode::orphan_box, orphan->id));
    REQUIRE_FALSE(HasWarningForBox(r, RoutingGraph::ValidationCode::dead_branch_warning, orphan->id));
}

// ---------------------------------------------------------------------------
// Suite 6 — ValidationResult helpers
// ---------------------------------------------------------------------------

TEST_CASE("Validation – IsValid is true when no errors, even with warnings",
          "[Build][routing_validation]")
{
    RoutingGraph::ValidationResult r;
    r.AddWarning(RoutingGraph::ValidationCode::dead_branch_warning, 1, "test");
    REQUIRE(r.IsValid());
    REQUIRE(r.HasWarnings());
    REQUIRE_FALSE(r.IsClean());
}

TEST_CASE("Validation – IsClean is true only when no errors and no warnings",
          "[Build][routing_validation]")
{
    RoutingGraph::ValidationResult r;
    REQUIRE(r.IsClean());

    r.AddError(RoutingGraph::ValidationCode::cycle_detected, INVALID_BOX_ID, "cycle");
    REQUIRE_FALSE(r.IsClean());
}

TEST_CASE("Validation – Merge combines errors and warnings from two results",
          "[Build][routing_validation]")
{
    RoutingGraph::ValidationResult r1, r2;
    r1.AddError(RoutingGraph::ValidationCode::cycle_detected, INVALID_BOX_ID, "cycle");
    r2.AddWarning(RoutingGraph::ValidationCode::orphan_box, 5, "orphan");

    r1.Merge(r2);
    REQUIRE(r1.errors.size() == 1);
    REQUIRE(r1.warnings.size() == 1);
}

// ---------------------------------------------------------------------------
// Suite 7 — Pedalboard::Validate convenience wrapper
// ---------------------------------------------------------------------------

TEST_CASE("Pedalboard::Validate – default pedalboard is clean",
          "[Build][routing_validation]")
{
    Pedalboard pb = Pedalboard::MakeDefault();
    auto r = pb.Validate(2);
    REQUIRE(r.IsClean());
}

TEST_CASE("Pedalboard::Validate – skips output range check when no channels given",
          "[Build][routing_validation]")
{
    Pedalboard pb = Pedalboard::MakeDefault();
    auto r = pb.Validate(); // default = -1, no interface
    REQUIRE(r.IsValid());
}
