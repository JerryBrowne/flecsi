/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */
#ifndef FLECSI_UTIL_GRAPHVIZ_HH
#define FLECSI_UTIL_GRAPHVIZ_HH

#include <flecsi-config.h>

#include "flecsi/flog.hh"

#if !defined(FLECSI_ENABLE_GRAPHVIZ)
#error FLECSI_ENABLE_GRAPHVIZ not defined! This file depends on Graphviz!
#endif

#include <graphviz/gvc.h>

/// \cond core
namespace flecsi {
namespace util {
/// \defgroup graphviz Graphviz Support
/// Wrapper for GVC.
/// \ingroup utils
/// \{

// attribute strings
static constexpr const char * gv_graph = "graph";
static constexpr const char * gv_label_default = "";
static constexpr const char * gv_label = "label";
static constexpr const char * gv_color = "color";
static constexpr const char * gv_color_black = "black";
static constexpr const char * gv_penwidth = "penwidth";
static constexpr const char * gv_penwidth_default = "";
static constexpr const char * gv_shape = "shape";
static constexpr const char * gv_shape_default = "ellipse";
static constexpr const char * gv_style = "style";
static constexpr const char * gv_style_default = "";
static constexpr const char * gv_fill_color = "fillcolor";
static constexpr const char * gv_color_lightgrey = "lightgrey";
static constexpr const char * gv_font_color = "fontcolor";
static constexpr const char * gv_dir = "dir";
static constexpr const char * gv_dir_default = "forward";
static constexpr const char * gv_headport = "headport";
static constexpr const char * gv_headport_default = "c";
static constexpr const char * gv_tailport = "tailport";
static constexpr const char * gv_tailport_default = "c";
static constexpr const char * gv_arrowsize = "arrowsize";
static constexpr const char * gv_arrowsize_default = "0.75";
static constexpr const char * gv_arrowhead = "arrowhead";
static constexpr const char * gv_arrowhead_default = "normal";
static constexpr const char * gv_arrowtail = "arrowtail";
static constexpr const char * gv_arrowtail_default = "normal";

#define GV_GRAPH const_cast<char *>(gv_graph)
#define GV_LABEL const_cast<char *>(gv_label)
#define GV_LABEL_DEFAULT const_cast<char *>(gv_label_default)
#define GV_PENWIDTH const_cast<char *>(gv_penwidth)
#define GV_PENWIDTH_DEFAULT const_cast<char *>(gv_penwidth_default)
#define GV_COLOR const_cast<char *>(gv_color)
#define GV_COLOR_BLACK const_cast<char *>(gv_color_black)
#define GV_SHAPE const_cast<char *>(gv_shape)
#define GV_SHAPE_DEFAULT const_cast<char *>(gv_shape_default)
#define GV_STYLE const_cast<char *>(gv_style)
#define GV_STYLE_DEFAULT const_cast<char *>(gv_style_default)
#define GV_FILL_COLOR const_cast<char *>(gv_fill_color)
#define GV_COLOR_LIGHTGREY const_cast<char *>(gv_color_lightgrey)
#define GV_FONT_COLOR const_cast<char *>(gv_font_color)
#define GV_DIR const_cast<char *>(gv_dir)
#define GV_DIR_DEFAULT const_cast<char *>(gv_dir_default)
#define GV_HEADPORT const_cast<char *>(gv_headport)
#define GV_HEADPORT_DEFAULT const_cast<char *>(gv_headport_default)
#define GV_TAILPORT const_cast<char *>(gv_tailport)
#define GV_TAILPORT_DEFAULT const_cast<char *>(gv_tailport_default)
#define GV_ARROWSIZE const_cast<char *>(gv_arrowsize)
#define GV_ARROWSIZE_DEFAULT const_cast<char *>(gv_arrowsize_default)
#define GV_ARROWHEAD const_cast<char *>(gv_arrowhead)
#define GV_ARROWHEAD_DEFAULT const_cast<char *>(gv_arrowhead_default)
#define GV_ARROWTAIL const_cast<char *>(gv_arrowtail)
#define GV_ARROWTAIL_DEFAULT const_cast<char *>(gv_arrowtail_default)

const int ag_create(1);
const int ag_access(0);

/// Class for creating Graphviz trees.
class graphviz
{
public:
  graphviz() : gvc_(nullptr), graph_(nullptr) {
    gvc_ = gvContext();
    clear();
  } // graphviz

  ~graphviz() {
    if(graph_ != nullptr) {
      gvFreeLayout(gvc_, graph_);
      agclose(graph_);
    } // if

    gvFreeContext(gvc_);
  } // ~graphviz

  /// Clear the graph.  This call is a little counter-intuitive.  It frees
  /// the currently allocated graph and creates a new empty one in its place.
  /// Therefore, there is always a graph instance available.
  void clear() {
    if(graph_ != nullptr) {
      gvFreeLayout(gvc_, graph_);
      agclose(graph_);
    } // if

    graph_ = agopen(GV_GRAPH, Agdirected, nullptr);
    agattr(
      graph_, AGRAPH, const_cast<char *>("nodesep"), const_cast<char *>(".5"));

    // set default node attributes
    agattr(graph_, AGNODE, GV_LABEL, GV_LABEL_DEFAULT);
    agattr(graph_, AGNODE, GV_PENWIDTH, GV_PENWIDTH_DEFAULT);
    agattr(graph_, AGNODE, GV_COLOR, GV_COLOR_BLACK);
    agattr(graph_, AGNODE, GV_SHAPE, GV_SHAPE_DEFAULT);
    agattr(graph_, AGNODE, GV_STYLE, GV_STYLE_DEFAULT);
    agattr(graph_, AGNODE, GV_FILL_COLOR, GV_COLOR_LIGHTGREY);
    agattr(graph_, AGNODE, GV_FONT_COLOR, GV_COLOR_BLACK);

    // set default edge attributes
    agattr(graph_, AGEDGE, GV_DIR, GV_DIR_DEFAULT);
    agattr(graph_, AGEDGE, GV_LABEL, GV_LABEL_DEFAULT);
    agattr(graph_, AGEDGE, GV_PENWIDTH, GV_PENWIDTH_DEFAULT);
    agattr(graph_, AGEDGE, GV_COLOR, GV_COLOR_BLACK);
    agattr(graph_, AGEDGE, GV_STYLE, GV_STYLE_DEFAULT);
    agattr(graph_, AGEDGE, GV_FILL_COLOR, GV_COLOR_BLACK);
    agattr(graph_, AGEDGE, GV_FONT_COLOR, GV_COLOR_BLACK);
    agattr(graph_, AGEDGE, GV_HEADPORT, GV_HEADPORT_DEFAULT);
    agattr(graph_, AGEDGE, GV_TAILPORT, GV_TAILPORT_DEFAULT);
    agattr(graph_, AGEDGE, GV_ARROWSIZE, GV_ARROWSIZE_DEFAULT);
    agattr(graph_, AGEDGE, GV_ARROWHEAD, GV_ARROWHEAD_DEFAULT);
    agattr(graph_, AGEDGE, GV_ARROWTAIL, GV_ARROWTAIL_DEFAULT);
  } // clear

  /// Add a node to the graph.
  /// \param label if non-null, be used for the display name of the node.
  Agnode_t * add_node(const char * name, const char * label = nullptr) {
    char buffer[1024];
    sprintf(buffer, "%s", name);
    Agnode_t * node = agnode(graph_, buffer, ag_create);

    if(label != nullptr) {
      char attr[1024];
      sprintf(attr, "%s", "label");
      sprintf(buffer, "%s", label);
      agset(node, attr, buffer);
    } // if

    return node;
  } // add_node

  Agnode_t * node(const char * name) {
    char buffer[1024];
    sprintf(buffer, "%s", name);
    return agnode(graph_, buffer, ag_access);
  } // node

  /// Remove a node from the graph.
  void remove_node(const char * name) {
    char buffer[1024];
    sprintf(buffer, "%s", name);
    Agnode_t * node = agfindnode(graph_, buffer);

    if(node != nullptr) {
      agdelete(graph_, node);
    } // if
  } // remove_node

  /// Set a node attribute.
  void
  set_node_attribute(const char * name, const char * attr, const char * value) {
    char buffer[1024];
    sprintf(buffer, "%s", name);
    Agnode_t * node = agnode(graph_, buffer, ag_access);

    if(node == nullptr) {
      flog(warn) << "node " << name << " does not exist";
      return;
    } // if

    char _attr[1024];
    char _value[1024];
    sprintf(_attr, "%s", attr);
    sprintf(_value, "%s", value);
    agset(node, _attr, _value);
  } // set_node_attribute

  void
  set_node_attribute(Agnode_t * node, const char * attr, const char * value) {
    char _attr[1024];
    char _value[1024];
    sprintf(_attr, "%s", attr);
    sprintf(_value, "%s", value);
    agset(node, _attr, _value);
  } // set_node_attribute

  /// Append to node label.
  void set_label(const char * name, const char * label) {
    char buffer[1024];
    sprintf(buffer, "%s", name);
    Agnode_t * node = agnode(graph_, buffer, ag_access);

    if(node == nullptr) {
      flog(warn) << "node " << name << " does not exist";
      return;
    } // if

    set_node_attribute(name, "label", label);
  } // append_node_label

  /// Get a node attribute.
  char * get_node_attribute(const char * name, const char * attr) {
    char buffer[1024];
    sprintf(buffer, "%s", name);
    Agnode_t * node = agnode(graph_, buffer, ag_access);

    if(node == nullptr) {
      flog(warn) << "node " << name << " does not exist";
      return nullptr;
    } // if

    char _attr[1024];
    sprintf(_attr, "%s", attr);
    return agget(node, _attr);
  } // set_node_attribute

  int inedge_count(const char * name) {
    char buffer[1024];
    sprintf(buffer, "%s", name);
    Agnode_t * node = agnode(graph_, buffer, ag_access);

    if(node == nullptr) {
      flog(warn) << "node " << name << " does not exist";
      return -1;
    } // if

    return agdegree(graph_, node, true, false);
  } // edge_count

  /// Add an edge to the graph.
  Agedge_t * add_edge(Agnode_t * parent, Agnode_t * child) {
    return agedge(graph_, parent, child, nullptr, ag_create);
  } // add_edge

  void
  set_edge_attribute(Agedge_t * edge, const char * attr, const char * value) {
    char _attr[1024];
    char _value[1024];
    sprintf(_attr, "%s", attr);
    sprintf(_value, "%s", value);
    agset(edge, _attr, _value);
  } // set_edge_attribute

  /// Compute a layout using the specified engine.
  void layout(const char * engine) {
    char _engine[1024];
    sprintf(_engine, "%s", engine);
    // FIXME: Error handling
    gvLayout(gvc_, graph_, _engine);
  } // layout

  /// Write the current layout to file.

  // FIXME: May need to check that a valid layout exists.
  void write(const std::string & name) {
    write(name.c_str());
  } // write

  void write(const char * name) {
    FILE * file = fopen(name, "w");

    if(name == nullptr) {
      flog_fatal("failed opening " << name);
    } // if

    agwrite(graph_, file);
    fclose(file);
  } // write

private:
  // private data members
  GVC_t * gvc_;
  Agraph_t * graph_;

}; // class graphviz

/// \}
} // namespace util
} // namespace flecsi
/// \endcond

#endif
