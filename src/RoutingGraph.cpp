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
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// ---------------------------------------------------------------------------
// RoutingGraph.cpp — US-06: JSON serialization for RoutingGraph, Box,
// Connection, and RgControlValue.
//
// Format notes:
//   - BoxType is serialized as a string ("Plugin", "Merge", "Ab", "Output").
//   - boxes_ is written as an array of Box objects (not shared_ptr — we
//     unwrap on write and re-wrap on read).
//   - Unknown JSON keys are silently skipped for forward compatibility.
// ---------------------------------------------------------------------------

#include "pch.h"
#include "RoutingGraph.hpp"

using namespace pipedal;

// ---------------------------------------------------------------------------
// Helpers — write a comma-separated list of members inside an already-opened
// object.  The first_ flag tracks whether we need a leading comma.
// ---------------------------------------------------------------------------

static void wm(json_writer& w, bool& first, const char* name, auto& value)
{
    if (!first) { w.output_stream() << ',' << w.CRLF; }
    first = false;
    w.indent();
    w.write_member(name, value);
}

static BoxType BoxTypeFromString(const std::string& s)
{
    if (s == "Merge")  return BoxType::Merge;
    if (s == "Ab")     return BoxType::Ab;
    if (s == "Output") return BoxType::Output;
    return BoxType::Plugin;
}

static const char* BoxTypeToString(BoxType t)
{
    switch (t) {
    case BoxType::Merge:  return "Merge";
    case BoxType::Ab:     return "Ab";
    case BoxType::Output: return "Output";
    default:              return "Plugin";
    }
}

// ---------------------------------------------------------------------------
// RgControlValue
// ---------------------------------------------------------------------------

void RgControlValue::write_json(json_writer& writer) const
{
    writer.start_object();
    bool first = true;
    wm(writer, first, "key",   key);
    wm(writer, first, "value", value);
    writer.end_object();
}

void RgControlValue::read_json(json_reader& reader)
{
    reader.start_object();
    while (reader.peek() != '}')
    {
        std::string name = reader.read_string();
        reader.consume(':');
        if      (name == "key")   reader.read(&key);
        else if (name == "value") reader.read(&value);
        else                      reader.skip_property();
        if (reader.peek() == ',') reader.consume(',');
    }
    reader.end_object();
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

void Connection::write_json(json_writer& writer) const
{
    writer.start_object();
    bool first = true;
    wm(writer, first, "from", from);
    wm(writer, first, "to",   to);
    writer.end_object();
}

void Connection::read_json(json_reader& reader)
{
    reader.start_object();
    while (reader.peek() != '}')
    {
        std::string name = reader.read_string();
        reader.consume(':');
        if      (name == "from") reader.read(&from);
        else if (name == "to")   reader.read(&to);
        else                     reader.skip_property();
        if (reader.peek() == ',') reader.consume(',');
    }
    reader.end_object();
}

// ---------------------------------------------------------------------------
// Box
// ---------------------------------------------------------------------------

void Box::write_json(json_writer& writer) const
{
    std::string typeStr = BoxTypeToString(type);

    writer.start_object();
    bool first = true;
    wm(writer, first, "id",                 id);
    wm(writer, first, "type",               typeStr);
    wm(writer, first, "isEnabled",          isEnabled);
    wm(writer, first, "title",              title);
    wm(writer, first, "iconColor",          iconColor);
    wm(writer, first, "pluginUri",          pluginUri);
    wm(writer, first, "pluginName",         pluginName);
    wm(writer, first, "controlValues",      controlValues);
    wm(writer, first, "outputChannelIndex", outputChannelIndex);
    wm(writer, first, "mergeVolume",        mergeVolume);
    wm(writer, first, "mergePanL",          mergePanL);
    wm(writer, first, "mergePanR",          mergePanR);
    wm(writer, first, "abSelectedInput",    abSelectedInput);
    writer.end_object();
}

void Box::read_json(json_reader& reader)
{
    reader.start_object();
    while (reader.peek() != '}')
    {
        std::string name = reader.read_string();
        reader.consume(':');

        if      (name == "id")                 reader.read(&id);
        else if (name == "type")               { std::string t; reader.read(&t); type = BoxTypeFromString(t); }
        else if (name == "isEnabled")          reader.read(&isEnabled);
        else if (name == "title")              reader.read(&title);
        else if (name == "iconColor")          reader.read(&iconColor);
        else if (name == "pluginUri")          reader.read(&pluginUri);
        else if (name == "pluginName")         reader.read(&pluginName);
        else if (name == "controlValues")      reader.read(&controlValues);
        else if (name == "outputChannelIndex") reader.read(&outputChannelIndex);
        else if (name == "mergeVolume")        reader.read(&mergeVolume);
        else if (name == "mergePanL")          reader.read(&mergePanL);
        else if (name == "mergePanR")          reader.read(&mergePanR);
        else if (name == "abSelectedInput")    reader.read(&abSelectedInput);
        else                                   reader.skip_property();

        if (reader.peek() == ',') reader.consume(',');
    }
    reader.end_object();
}

// ---------------------------------------------------------------------------
// RoutingGraph
//
// boxes_ is vector<shared_ptr<Box>>. We serialize as an array of Box objects
// (unwrapping on write, re-wrapping on read).
// ---------------------------------------------------------------------------

void RoutingGraph::write_json(json_writer& writer) const
{
    // Flatten shared_ptrs to plain Box values for the array writer.
    std::vector<Box> flatBoxes;
    flatBoxes.reserve(boxes_.size());
    for (const auto& b : boxes_)
        if (b) flatBoxes.push_back(*b);

    writer.start_object();
    bool first = true;
    wm(writer, first, "name",        name);
    wm(writer, first, "nextBoxId",   nextBoxId_);
    wm(writer, first, "boxes",       flatBoxes);
    wm(writer, first, "connections", connections_);
    writer.end_object();
}

void RoutingGraph::read_json(json_reader& reader)
{
    std::vector<Box> flatBoxes;

    reader.start_object();
    while (reader.peek() != '}')
    {
        std::string nm = reader.read_string();
        reader.consume(':');

        if      (nm == "name")        reader.read(&name);
        else if (nm == "nextBoxId")   reader.read(&nextBoxId_);
        else if (nm == "boxes")       reader.read(&flatBoxes);
        else if (nm == "connections") reader.read(&connections_);
        else                          reader.skip_property();

        if (reader.peek() == ',') reader.consume(',');
    }
    reader.end_object();

    // Re-wrap each Box in a shared_ptr.
    boxes_.clear();
    boxes_.reserve(flatBoxes.size());
    for (auto& b : flatBoxes)
        boxes_.push_back(std::make_shared<Box>(std::move(b)));
}
