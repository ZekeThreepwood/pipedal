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
// RoutingGraphTest.cpp
//
// Tests for US-01 (routing domain model) and US-02 (stable identity +
// traversal helpers).
//
// Tags: [Build][routing_graph]
// ---------------------------------------------------------------------------

#include "pch.h"
#include <catch/catch.hpp>
#include "RoutingGraph.hpp"
#include <cmath>

using namespace pipedal;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static constexpr float DB_TOLERANCE = 0.01f;

static float DbFromLinear(float linear)
{
    return 20.0f * std::log10(linear);
}

// ---------------------------------------------------------------------------
// Suite 1 — Box identity and types
// ---------------------------------------------------------------------------

TEST_CASE("RoutingGraph – each box gets a unique stable ID",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto p1 = g.MakePluginBox("lv2:PluginA");
    auto p2 = g.MakePluginBox("lv2:PluginB");
    auto out = g.MakeOutputBox(0);

    REQUIRE(p1->id != INVALID_BOX_ID);
    REQUIRE(p2->id != INVALID_BOX_ID);
    REQUIRE(out->id != INVALID_BOX_ID);
    REQUIRE(p1->id != p2->id);
    REQUIRE(p1->id != out->id);
    REQUIRE(p2->id != out->id);
}

TEST_CASE("RoutingGraph – box types are correct after Make* helpers",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto plugin = g.MakePluginBox("lv2:Plugin");
    auto merge  = g.MakeMergeBox();
    auto ab     = g.MakeAbBox();
    auto output = g.MakeOutputBox(1);

    REQUIRE(plugin->isPlugin());
    REQUIRE_FALSE(plugin->isMerge());

    REQUIRE(merge->isMerge());
    REQUIRE_FALSE(merge->isPlugin());

    REQUIRE(ab->isAb());
    REQUIRE_FALSE(ab->isOutput());

    REQUIRE(output->isOutput());
    REQUIRE(output->outputChannelIndex == 1);
}

TEST_CASE("RoutingGraph – FindBox returns correct box or nullptr",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto p = g.MakePluginBox("lv2:X");
    g.AddBox(p);

    REQUIRE(g.FindBox(p->id) == p.get());
    REQUIRE(g.FindBox(INVALID_BOX_ID) == nullptr);
    REQUIRE(g.FindBox(9999) == nullptr);
}

// ---------------------------------------------------------------------------
// Suite 2 — Connections (fan-out / fan-in)
// ---------------------------------------------------------------------------

TEST_CASE("RoutingGraph – Connect creates an edge, duplicate is ignored",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);

    g.Connect(a->id, b->id);
    REQUIRE(g.OutgoingCount(a->id) == 1);

    g.Connect(a->id, b->id); // duplicate
    REQUIRE(g.OutgoingCount(a->id) == 1);
}

TEST_CASE("RoutingGraph – Disconnect removes an edge",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);

    g.Connect(a->id, b->id);
    g.Disconnect(a->id, b->id);
    REQUIRE(g.OutgoingCount(a->id) == 0);
    REQUIRE(g.IncomingCount(b->id) == 0);
}

TEST_CASE("RoutingGraph – fan-out: one box feeding multiple downstream",
          "[Build][routing_graph]")
{
    // amp → delay
    //     → chorus
    RoutingGraph g;
    auto amp    = g.MakePluginBox("lv2:Amp");    g.AddBox(amp);
    auto delay  = g.MakePluginBox("lv2:Delay");  g.AddBox(delay);
    auto chorus = g.MakePluginBox("lv2:Chorus"); g.AddBox(chorus);

    g.Connect(amp->id, delay->id);
    g.Connect(amp->id, chorus->id);

    REQUIRE(g.OutgoingCount(amp->id) == 2);
    REQUIRE(g.IncomingCount(delay->id) == 1);
    REQUIRE(g.IncomingCount(chorus->id) == 1);

    auto downstream = g.GetDownstream(amp->id);
    REQUIRE(downstream.size() == 2);
}

TEST_CASE("RoutingGraph – fan-in: multiple boxes feeding one downstream",
          "[Build][routing_graph]")
{
    // delay  → cab
    // chorus ↗
    RoutingGraph g;
    auto delay  = g.MakePluginBox("lv2:Delay");  g.AddBox(delay);
    auto chorus = g.MakePluginBox("lv2:Chorus"); g.AddBox(chorus);
    auto cab    = g.MakePluginBox("lv2:Cab");    g.AddBox(cab);

    g.Connect(delay->id,  cab->id);
    g.Connect(chorus->id, cab->id);

    REQUIRE(g.IncomingCount(cab->id) == 2);
    auto upstream = g.GetUpstream(cab->id);
    REQUIRE(upstream.size() == 2);
}

TEST_CASE("RoutingGraph – RemoveBox cleans up all its connections",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);
    auto c = g.MakePluginBox("lv2:C"); g.AddBox(c);

    g.Connect(a->id, b->id);
    g.Connect(b->id, c->id);

    g.RemoveBox(b->id);

    REQUIRE(g.FindBox(b->id) == nullptr);
    REQUIRE(g.OutgoingCount(a->id) == 0);
    REQUIRE(g.IncomingCount(c->id) == 0);
}

// ---------------------------------------------------------------------------
// Suite 3 — Implicit fan-in gain compensation
// ---------------------------------------------------------------------------

TEST_CASE("RoutingGraph – ImplicitFanInCompensationDb is 0 for N=1",
          "[Build][routing_graph]")
{
    REQUIRE(RoutingGraph::ImplicitFanInCompensationDb(1) == Approx(0.0f).margin(DB_TOLERANCE));
}

TEST_CASE("RoutingGraph – ImplicitFanInCompensationDb is ~-6dB for N=2",
          "[Build][routing_graph]")
{
    float db = RoutingGraph::ImplicitFanInCompensationDb(2);
    REQUIRE(db == Approx(-6.0206f).margin(DB_TOLERANCE));
}

TEST_CASE("RoutingGraph – ImplicitFanInCompensationDb is ~-9.54dB for N=3",
          "[Build][routing_graph]")
{
    float db = RoutingGraph::ImplicitFanInCompensationDb(3);
    REQUIRE(db == Approx(-9.5424f).margin(DB_TOLERANCE));
}

TEST_CASE("RoutingGraph – ImplicitFanInCompensationDb is ~-12dB for N=4",
          "[Build][routing_graph]")
{
    float db = RoutingGraph::ImplicitFanInCompensationDb(4);
    REQUIRE(db == Approx(-12.0412f).margin(DB_TOLERANCE));
}

TEST_CASE("RoutingGraph – ImplicitFanInCompensationDb follows -20*log10(N)",
          "[Build][routing_graph]")
{
    for (int n = 2; n <= 8; ++n)
    {
        float expected = -20.0f * std::log10(static_cast<float>(n));
        REQUIRE(RoutingGraph::ImplicitFanInCompensationDb(n) == Approx(expected).margin(DB_TOLERANCE));
    }
}

// ---------------------------------------------------------------------------
// Suite 4 — Acyclicity
// ---------------------------------------------------------------------------

TEST_CASE("RoutingGraph – simple serial chain is acyclic",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);
    auto o = g.MakeOutputBox(0);       g.AddBox(o);

    g.Connect(a->id, b->id);
    g.Connect(b->id, o->id);

    REQUIRE(g.IsAcyclic());
}

TEST_CASE("RoutingGraph – fan-out then implicit fan-in is acyclic",
          "[Build][routing_graph]")
{
    // amp → delay  → cab → out
    //     → chorus ↗
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

    REQUIRE(g.IsAcyclic());
}

TEST_CASE("RoutingGraph – direct cycle is detected",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);

    g.Connect(a->id, b->id);
    g.Connect(b->id, a->id); // cycle

    REQUIRE_FALSE(g.IsAcyclic());
}

TEST_CASE("RoutingGraph – self-loop is detected as cycle",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    g.Connect(a->id, a->id);
    REQUIRE_FALSE(g.IsAcyclic());
}

// ---------------------------------------------------------------------------
// Suite 5 — Dead branch detection
// ---------------------------------------------------------------------------

TEST_CASE("RoutingGraph – fully connected graph has no dead branches",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto o = g.MakeOutputBox(0);       g.AddBox(o);
    g.Connect(a->id, o->id);

    REQUIRE(g.FindDeadBranches().empty());
}

TEST_CASE("RoutingGraph – disconnected plugin box is a dead branch",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a); // connected
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b); // dead
    auto o = g.MakeOutputBox(0);       g.AddBox(o);

    g.Connect(a->id, o->id);
    // b is not connected to anything

    auto dead = g.FindDeadBranches();
    REQUIRE(std::find(dead.begin(), dead.end(), b->id) != dead.end());
    REQUIRE(std::find(dead.begin(), dead.end(), a->id) == dead.end());
}

TEST_CASE("RoutingGraph – branch that reaches output indirectly is not dead",
          "[Build][routing_graph]")
{
    // amp → delay → cab → out
    //     → chorus ↗          (chorus reaches out via cab)
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

    REQUIRE(g.FindDeadBranches().empty());
}

TEST_CASE("RoutingGraph – branch terminating at wrong output is not dead",
          "[Build][routing_graph]")
{
    // Scarlett 2i2 style: two amp chains to two physical outputs
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

    REQUIRE(g.FindDeadBranches().empty());
    REQUIRE(g.IsAcyclic());
}

// ---------------------------------------------------------------------------
// Suite 6 — Traversal helpers
// ---------------------------------------------------------------------------

TEST_CASE("RoutingGraph – WalkDownstream visits all reachable boxes",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);
    auto c = g.MakePluginBox("lv2:C"); g.AddBox(c);
    auto o = g.MakeOutputBox(0);       g.AddBox(o);

    g.Connect(a->id, b->id);
    g.Connect(b->id, c->id);
    g.Connect(c->id, o->id);

    std::vector<BoxId> visited;
    g.WalkDownstream(a->id, [&](Box* box){ visited.push_back(box->id); });

    REQUIRE(visited.size() == 4); // a, b, c, out
}

TEST_CASE("RoutingGraph – WalkUpstream visits all upstream boxes",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);
    auto c = g.MakePluginBox("lv2:C"); g.AddBox(c);

    g.Connect(a->id, b->id);
    g.Connect(b->id, c->id);

    std::vector<BoxId> visited;
    g.WalkUpstream(c->id, [&](Box* box){ visited.push_back(box->id); });

    REQUIRE(visited.size() == 3); // c, b, a
}

TEST_CASE("RoutingGraph – GetRootBoxes returns boxes with no incoming connections",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a); // root
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b); // root
    auto c = g.MakePluginBox("lv2:C"); g.AddBox(c); // not root

    g.Connect(a->id, c->id);
    g.Connect(b->id, c->id);

    auto roots = g.GetRootBoxes();
    REQUIRE(roots.size() == 2);
}

TEST_CASE("RoutingGraph – TopologicalSort returns all boxes for acyclic graph",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);
    auto o = g.MakeOutputBox(0);       g.AddBox(o);

    g.Connect(a->id, b->id);
    g.Connect(b->id, o->id);

    auto sorted = g.TopologicalSort();
    REQUIRE(sorted.size() == 3);
    // a must come before b, b before o
    auto posA = std::find(sorted.begin(), sorted.end(), a.get());
    auto posB = std::find(sorted.begin(), sorted.end(), b.get());
    auto posO = std::find(sorted.begin(), sorted.end(), o.get());
    REQUIRE(posA < posB);
    REQUIRE(posB < posO);
}

TEST_CASE("RoutingGraph – TopologicalSort returns empty for cyclic graph",
          "[Build][routing_graph]")
{
    RoutingGraph g;
    auto a = g.MakePluginBox("lv2:A"); g.AddBox(a);
    auto b = g.MakePluginBox("lv2:B"); g.AddBox(b);
    g.Connect(a->id, b->id);
    g.Connect(b->id, a->id);

    REQUIRE(g.TopologicalSort().empty());
}

// ---------------------------------------------------------------------------
// Suite 7 — MakeDefault
// ---------------------------------------------------------------------------

TEST_CASE("RoutingGraph – MakeDefault produces a valid acyclic graph",
          "[Build][routing_graph]")
{
    auto g = RoutingGraph::MakeDefault(2);

    REQUIRE(g.IsAcyclic());
    REQUIRE(g.GetAllOutputBoxes().size() == 2);
    REQUIRE_FALSE(g.FindDeadBranches().empty() && g.GetAllPluginBoxes().empty());
}

TEST_CASE("RoutingGraph – MakeDefault single output has no dead branches",
          "[Build][routing_graph]")
{
    auto g = RoutingGraph::MakeDefault(1);
    // The default empty plugin is connected to Out 1, so no dead branches.
    REQUIRE(g.FindDeadBranches().empty());
}
