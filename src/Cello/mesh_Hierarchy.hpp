// See LICENSE_CELLO file for license and copyright information

/// @file     mesh_Hierarchy.hpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     Tue Nov 10 15:38:40 PST 2009
/// @brief    [\ref Mesh] Declaration of the Hierarchy class

#ifndef MESH_HIERARCHY_HPP
#define MESH_HIERARCHY_HPP

class Factory;
#ifdef CONFIG_USE_CHARM
class CProxy_CommBlock;
#endif

class Hierarchy {

  /// @class    Hierarchy
  /// @ingroup  Mesh
  /// @brief    [\ref Mesh] Adaptive mesh refinement hierarchy

  friend class IoHierarchy;

public: // interface

  /// Empty constructor for Charm++ pup()
  Hierarchy() throw() { }
  
  /// Initialize a Hierarchy object
  Hierarchy ( const Factory * factory,
	      int dimension, int refinement
#ifdef REMOVE_PATCH
	      ,int process_first, int process_last_plus
#endif /* REMOVE_PATCH */
) throw ();

  /// Delete the Hierarchy object
  virtual ~Hierarchy() throw ();

#ifdef CONFIG_USE_CHARM
  /// CHARM++ Pack / Unpack function
  void pup (PUP::er &p);
#endif

  //----------------------------------------------------------------------

  /// Set domain lower extent
  void set_lower(double x, double y, double z) throw ();

  /// Set domain upper extent
  void set_upper(double x, double y, double z) throw ();
  
  /// Set root-level grid size
  void set_root_size(int nx, int ny, int nz) throw ();

  //----------------------------------------------------------------------

  /// Return dimension
  int dimension() const throw ();

  /// Return domain lower extent
  void lower(double * x, double * y = 0, double * z = 0) const throw ();

  /// Return domain upper extent
  void upper(double * x, double * y = 0, double * z = 0) const throw ();

  /// Return root-level grid size
  void root_size(int * nx, int * ny = 0, int * nz = 0) const throw ();

  //----------------------------------------------------------------------

#ifdef REMOVE_PATCH

# ifdef CONFIG_USE_CHARM
  /// Return pointer to the CommBlock CHARM++ chare array
  CProxy_CommBlock * block_array() const throw()
  //  { if (block_exists_) return block_array_; else return 0;}
  { return block_array_;}

# else
  /// Return the total number of local blocks
  size_t num_local_blocks() const throw();

  /// Return the ith local CommBlock
  CommBlock * local_block(size_t i) const throw();
# endif

  /// Return the total number of blocks
  size_t num_blocks() const throw()
  { 
    WARNING("Hierarchy::num_blocks()",
	    "num_blocks_ initialization not implemented for AMR");
    return num_blocks_; 
  }

  void create_forest (FieldDescr   * field_descr,
		      int nx, int ny, int nz,
		      int nbx, int nby, int nbz,
		      bool allocate_blocks  = true,
		      int process_first     = 0, 
		      int process_last_plus = -1) throw();


#else /* REMOVE_PATCH */

  /// Return the total number of local patches
  size_t num_patches() const throw();

  /// Return the ith patch.  Patch[0] is the root
  Patch * patch(size_t i) throw();

  /// Return the ith patch
  Patch * patch(size_t i) const throw();

  /// Create the initial root patch
  void create_root_patch (FieldDescr   * field_descr,
			  int nx, int ny, int nz,
			  int nbx, int nby, int nbz,
			  bool allocate_blocks  = true,
			  int process_first     = 0, 
			  int process_last_plus = -1) throw();

#endif /* REMOVE_PATCH */

  /// Return the factory object associated with the Hierarchy
  const Factory * factory () const throw()
  { return factory_; }

#ifdef REMOVE_PATCH
  /// Return the layout of the patch, describing processes and blocking
  Layout * layout () throw();

  /// Return the layout of the patch, describing processes and blocking
  const Layout * layout () const throw();

  const GroupProcess * group_process()  const throw()
  { return group_process_; };

#endif /* REMOVE_PATCH */


protected: // attributes

  /// Factory for creating Simulations, Hierarchies, Patches and Blocks
  /// [abstract factory design pattern]
  Factory * factory_;

  /// Dimension of the hierarchy [ used for Charm++ pup() of Tree ]
  int dimension_;
  
  /// Refinement of the hierarchy [ used for Charm++ pup() of Tree ]
  int refinement_;

#ifdef REMOVE_PATCH

  int num_blocks_; 

  /// Array of CommBlocks 
# ifdef CONFIG_USE_CHARM
  CProxy_CommBlock * block_array_;
  bool           block_exists_;
  Loop           block_loop_;
# else
  std::vector<CommBlock * > block_;
# endif

#else /* REMOVE_PATCH */
  
  /// Number of patches (redundant with patch_tree_)
  int patch_count_;

  /// List of local patches
  Tree * patch_tree_;

#endif /* REMOVE_PATCH */

  /// Size of the root grid
  int root_size_[3];

  /// Lower extent of the hierarchy
  double lower_[3];

  /// Upper extent of the hierarchy
  double upper_[3];

#ifdef REMOVE_PATCH

  /// Parallel Group for distributing the Mesh across processors
  GroupProcess * group_process_;

  /// Layout: describes blocking, processor range, and processor mapping 
  Layout * layout_;
#endif /* REMOVE_PATCH */
};


#endif /* MESH_HIERARCHY_HPP */

