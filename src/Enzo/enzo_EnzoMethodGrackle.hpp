// See LICENSE_CELLO file for license and copyright information

/// @file     enzo_EnzoMethodGrackle.hpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     Thu May 15 14:32:28 EDT 2014
/// @brief    [\ref Enzo] Declaration of EnzoMethodGrackle class
///
/// This class interfaces the Grackle primordial chemistry / cooling
/// library with Cello

#ifndef ENZO_ENZO_METHOD_GRACKLE_HPP
#define ENZO_ENZO_METHOD_GRACKLE_HPP


class EnzoMethodGrackle : public Method {

  /// @class    EnzoMethodGrackle
  /// @ingroup  Enzo
  ///
  /// This class interfaces the Grackle primordial chemistry / cooling
  /// library with Cello

public: // interface

  /// Create a new EnzoMethodGrackle object
  EnzoMethodGrackle(EnzoConfig * enzo_config,
                    const FieldDescr * field_descr);

  /// Charm++ PUP::able declarations
  PUPable_decl(EnzoMethodGrackle);

  /// Charm++ PUP::able migration constructor
  EnzoMethodGrackle (CkMigrateMessage *m)
    : Method (m)
  {  }

  /// CHARM++ Pack / Unpack function
  void pup (PUP::er &p) ;

  /// Destructor ?
  // virtual ~EnzoMethodGrackle() throw() {};
  // virtual ~EnzoMethodGrackle() throw();

  /// Apply the method to advance a block one timestep
  virtual void compute( Block * block) throw();

  virtual std::string name () throw ()
  { return "grackle"; }

  /// Compute maximum timestep for this method
  virtual double timestep ( Block * block) const throw();

  virtual ~EnzoMethodGrackle() throw() {};

//protected: // methods

protected: // attributes

#ifdef CONFIG_USE_GRACKLE
  void compute_( EnzoBlock * enzo_block) throw();

  /// Grackle struct defining code units
  code_units units_;

#endif /* ENZO_ENZO_METHOD_GRACKLE_HPP */

};

#endif /* ENZO_ENZO_METHOD_GRACKLE_HPP */
