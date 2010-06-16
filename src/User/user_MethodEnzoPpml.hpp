// $Id: user_MethodEnzoPpml.hpp 1258 2010-03-02 01:07:36Z bordner $
// See LICENSE_ENZO file for license and copyright information

/// @file     user_MethodEnzoPpml.hpp
/// @author   James Bordner (jobordner@ucsd.edu) 
/// @date     Mon May 17 14:16:01 PDT 2010
/// @brief    Implementation of Enzo PPML method

#ifndef USER_ENZO_PPML_HPP
#define USER_ENZO_PPML_HPP

#include <vector>
#include <string>
#include "method.hpp"

class MethodEnzoPpml : public MethodHyperbolic {

  /// @class    MethodEnzoPpml
  /// @ingroup  Enzo
  /// @brief    Encapsulate Enzo's PPML hydro method

public: // interface

  /// Perform any method-specific initialization

  void initialize_method(DataDescr * data_descr) throw();

  /// Perform any method-specific finalizations steps, e.g. to
  /// deallocate any dynamically-allocated memory

  void finalize_method(DataDescr * data_descr) throw();

  /// Initialize PPM variable that may change.  Called once per
  /// block per timestep.

  void initialize_block (DataBlock * data_block) throw();

  /// Finalize PPM after advancing a block a timestep, e.g. to deallocate
  /// any dynamically-allocated variables

  void finalize_block (DataBlock * data_block) throw();

  /// Apply the method to advance a block one timestep 

  void advance_block( DataBlock * data_block,
			      double t, double dt ) throw(); 

  /// Refresh a block face's boundary / ghost zones given neighboring
  /// block face(s)

  void refresh_face() throw();

private: // attributes


};

#endif /* USER_ENZO_PPML_HPP */
