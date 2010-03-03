// $Id$
// See LICENSE_CELLO file for license and copyright information

#ifndef AMR_TREEK_HPP
#define AMR_TREEK_HPP

/// @file     amr_treek.hpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     2009-10-30
/// @brief    Include file for amr_tree[23]k.hpp

class TreeK {

  /// @class    TreeK
  /// @ingroup  Amr
  /// @brief    Base class for Tree[23]K classes

public: // interface

  /// Initialize a TreeK with refinement factor r
  TreeK(int r) : r_(r), levels_(0) {};

  /// Delete a TreeK object
  virtual ~TreeK() {};

  /// Refine down to array
  virtual  void refine
    (const int * level_array, 
     int ndx, int ndy, int ndz,
     int max_level, 
     bool full_nodes = true
     ) = 0;

  /// Refine nodes to remove level jumps
  virtual void balance(bool full_nodes = true)= 0;

  /// Replace uniformly-refined patch with single node
  virtual void optimize()= 0;
  
  /// Create an image of levels
  virtual float * create_image (int n, int line_width, int axis=0)= 0;

  /// Create a geomview file
  virtual void geomview (std::string filename) = 0;

  /// Return the number of levels
  int levels() { return levels_; }

protected: // attributes

  /// Refinement factor
  int r_;

  /// Number of levels in the tree
  int levels_;

};

#endif /* TREE_K_HPP */
