// See LICENSE_CELLO file for license and copyright information

/// @file     mesh_Block.cpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     Mon Feb 28 13:22:26 PST 2011
/// @brief    Implementation of the Block object

#include "cello.hpp"
#include "mesh.hpp"
#include "main.hpp"
#include "charm_simulation.hpp"

//#define TRACE_BLOCK

// #define DEBUG_ADAPT
// #define DEBUG_FIELD_FACE
// #define DEBUG_FACE

#ifdef DEBUG_FACE
#   define DEBUG_FACES(MSG) debug_faces_(MSG)
#else
#   define DEBUG_FACES(MSG) /* ... */
#endif

// KEEP CONSISTENT WITH _comm.hpp: phase_type
const char * phase_name[] = {
  "unknown",
  "initial_enter",
  "initial_exit",
  "adapt_enter",
  "adapt_called",
  "adapt_next",
  "adapt_end",
  "adapt_exit",
  "compute_enter",
  "compute_continue",
  "compute_exit",
  "refresh_enter",
  "refresh_exit",
  "stopping_enter",
  "stopping_exit",
  "output_enter",
  "output_exit",
  "restart",
  "balance",
  "exit"
};

//----------------------------------------------------------------------

Block::Block ( MsgRefine * msg )
  : CBase_Block(),
    data_(NULL),
    child_data_(NULL),
    level_next_(0),
    cycle_(0),
    time_(0.0),
    dt_(0.0),
    stop_(false),
    index_initial_(0),
    children_(),
    sync_coarsen_(),
    sync_count_(),
    sync_max_(),
    adapt_(),
#ifdef OLD_ADAPT
    child_face_level_curr_(),
    child_face_level_next_(),
#endif /* OLD_ADAPT */
    count_coarsen_(0),
    adapt_step_(0),
    coarsened_(false),
    is_leaf_(true),
    age_(0),
    name_(""),
    index_method_(-1),
    index_solver_(),
    refresh_()
{
  performance_start_(perf_block);
  init_refresh_();

  usesAtSync = true;
  init_refine_ (msg->index_,
	msg->nx_, msg->ny_, msg->nz_,
	msg->num_field_blocks_,
	msg->num_adapt_steps_,
	msg->cycle_, msg->time_,  msg->dt_,
	0, NULL, msg->refresh_type_,
	msg->num_face_level_, msg->face_level_);

  init_adapt_();

  index_.array(array_,array_+1,array_+2);

  apply_initial_(msg);

  delete msg;
  performance_stop_(perf_block);


}

//----------------------------------------------------------------------

Block::Block ( process_type ip_source )
  : CBase_Block(),
    data_(NULL),
    child_data_(NULL),
    level_next_(0),
    cycle_(0),
    time_(0.0),
    dt_(0.0),
    stop_(false),
    index_initial_(0),
    children_(),
    sync_coarsen_(),
    sync_count_(),
    sync_max_(),
    adapt_(),
#ifdef OLD_ADAPT
    child_face_level_curr_(),
    child_face_level_next_(),
#endif /* OLD_ADAPT */
    count_coarsen_(0),
    adapt_step_(0),
    coarsened_(false),
    is_leaf_(true),
    age_(0),
    name_(""),
    index_method_(-1),
    index_solver_(),
    refresh_()
{

  init_refresh_();
  usesAtSync = true;

  performance_start_(perf_block);

  thisIndex.array(array_,array_+1,array_+2);

  proxy_simulation[ip_source].p_get_msg_refine(thisIndex);
  
  performance_stop_(perf_block);
}

//----------------------------------------------------------------------

void Block::p_set_msg_refine(MsgRefine * msg)
{
  performance_start_(perf_block);

  init_refine_ (msg->index_,
	msg->nx_, msg->ny_, msg->nz_,
	msg->num_field_blocks_,
	msg->num_adapt_steps_,
	msg->cycle_, msg->time_,  msg->dt_,
	0, NULL, msg->refresh_type_,
	msg->num_face_level_, msg->face_level_);

  init_adapt_();

#ifdef TRACE_BLOCK
  {
  CkPrintf ("%d %s index TRACE_BLOCK p_set_msg_refine(MsgRefine)\n",  CkMyPe(),name().c_str());
  }
#endif

  apply_initial_(msg);
  
  performance_stop_(perf_block);

}

//----------------------------------------------------------------------

void Block::init_refine_
(
 Index index,
 int nx, int ny, int nz,
 int num_field_blocks,
 int num_adapt_steps,
 int cycle, double time, double dt,
 int narray, char * array, int refresh_type,
 int num_face_level, int * face_level)

{
  index_ = index;
  cycle_ = cycle;
  time_ = time;
  dt_ = dt;
  adapt_step_ = num_adapt_steps;

  // Enable Charm++ AtSync() dynamic load balancing

  Simulation * simulation = cello::simulation();

#ifdef CELLO_DEBUG
  index_.print("Block()",-1,2,false,simulation);
#endif

  Monitor * monitor = (simulation != NULL) ? simulation->monitor() : NULL;
  
  if ((monitor != NULL) && monitor->is_verbose()) {
    char buffer [80];
    sprintf (buffer,"Block() %s %d (%x %x %x) created",name().c_str(),
	     index.level(),index[0],index[1],index[2]);
    monitor->print("Adapt",buffer);
  }
  int ibx,iby,ibz;
  index.array(&ibx,&iby,&ibz);

  double xm,ym,zm;
  lower(&xm,&ym,&zm);
  double xp,yp,zp;
  upper(&xp,&yp,&zp);

  // Allocate block data

  data_ = new Data  (nx, ny, nz, 
		     num_field_blocks,
		     xm,xp, ym,yp, zm,zp);

  data_->allocate();

  child_data_ = NULL;

  // Update state

  set_state (cycle,time,dt,stop_);

  sync_coarsen_.reset();
  sync_coarsen_.set_stop(cello::num_children());

  // Initialize neighbor face levels

  if (num_face_level == 0) {

    adapt_.reset_face_level (Adapt::LevelType::curr);

#ifdef OLD_ADAPT    
    child_face_level_curr_.resize(cello::num_children()*27);
#endif

  } else {

#ifdef OLD_ADAPT
    child_face_level_curr_.resize(cello::num_children()*num_face_level);
#endif
    adapt_.copy_face_level(Adapt::LevelType::curr,face_level);

  }

#ifdef OLD_ADAPT
  for (size_t i=0; i<child_face_level_curr_.size(); i++) 
    child_face_level_curr_[i] = 0;
#endif  

#ifdef OLD_ADAPT
  initialize_child_face_levels_();
#endif  

  adapt_.update_next_from_curr();
#ifdef OLD_ADAPT
  child_face_level_next_ = child_face_level_curr_;
#endif  

  const int level = this->level();

  int na3[3];
  size_array(na3,na3+1,na3+2);

  int ic3[3] = {0,0,0};
  if (level > 0) index_.child(level,ic3,ic3+1,ic3+2);

  if (narray != 0) {

    // Create field face of refined data from parent
    int if3[3] = {0,0,0};
    int g3[3];
    cello::field_descr()->ghost_depth(0,g3,g3+1,g3+2);
    Refresh * refresh = new Refresh;
    refresh->add_all_data();
    
    FieldFace * field_face = create_face
      (if3, ic3, g3, refresh_fine, refresh, true);
    
#ifdef DEBUG_FIELD_FACE  
  CkPrintf ("%d %s:%d DEBUG_FIELD_FACE creating %p\n",
            CkMyPe(),__FILE__,__LINE__,(void*)field_face);
#endif

    // Copy refined field data
    field_face -> array_to_face (array, data()->field());

    delete field_face;

  }

  if (simulation) simulation->data_insert_block(this);
  
  const int np = data()->particle().num_particles();
  if (np > 0) {
    if (simulation) simulation->data_insert_particles(np);
  }

  if (level > 0) {

    control_sync_quiescence (CkIndex_Main::p_adapt_end());

  }

  // Do not migrate the root Block (0,0,0) level (0)
  setMigratable(! index_.is_root());

  DEBUG_FACES("Block()");

}

//----------------------------------------------------------------------

void Block::initialize()
{
  bool is_first_cycle = (cycle_ == cello::config()->initial_cycle);
  if (! cello::config()->initial_new) {
    if (is_first_cycle && level() <= 0) {
      CkCallback callback (CkIndex_Block::r_end_initialize(NULL), thisProxy);
#ifdef TRACE_BLOCK    
      CkPrintf ("%s %s:%d DEBUG_CONTRIBUTE r_end_initialize()\n",
                name().c_str(),__FILE__,__LINE__); fflush(stdout);
#endif    
      contribute(0,0,CkReduction::concat,callback);
    }
  }
}

//----------------------------------------------------------------------

void Block::pup(PUP::er &p)
{
  TRACEPUP;

  CBase_Block::pup(p);

  bool up = p.isUnpacking();

  if (up) data_ = new Data;
  p | *data_;

  // child_data_ may be NULL
  bool allocated=(child_data_ != NULL);
  p|allocated;
  if (allocated) {
    if (up) child_data_=new Data;
    // child_data_ guaranteed to be non-NULL: adding check for
    // Coverity static analysis
    if (child_data_) p|*child_data_;
  } else {
    child_data_ = NULL;
  }

  p | index_;
  PUParray(p,array_,3);
  p | level_next_;
  p | cycle_;
  p | time_;
  p | dt_;
  p | stop_;
  p | index_initial_;
  p | children_;
  p | sync_coarsen_;
  p | sync_count_;
  p | sync_max_;
  p | adapt_;
#ifdef OLD_ADAPT
  p | child_face_level_curr_;
  p | child_face_level_next_;
#endif /* OLD_ADAPT */
  p | count_coarsen_;
  p | adapt_step_;
  p | coarsened_;
  p | is_leaf_;
  p | age_;
  p | name_;
  p | index_method_;
  p | index_solver_;
  p | refresh_;
  // SKIP method_: initialized when needed

  if (up) DEBUG_FACES("PUP");

  if (up) {
    Simulation * simulation = cello::simulation();
    if (simulation != NULL) simulation->data_insert_block(this);    
  }
  p | refresh_sync_list_;
}

//----------------------------------------------------------------------

ItFace Block::it_face
(int min_face_rank,
 Index index,
 const int * ic3,
 const int * if3) throw()
{
  int rank = cello::rank();
  int n3[3];
  size_array(n3,n3+1,n3+2);
  bool periodic[3];
  periodicity(periodic);
  return ItFace (rank,min_face_rank,periodic,n3,index,ic3,if3);
}

//----------------------------------------------------------------------

ItNeighbor Block::it_neighbor (int min_face_rank, Index index,
			       int neighbor_type,
			       int min_level, int coarse_level) throw()
{
  int n3[3];
  size_array(&n3[0],&n3[1],&n3[2]);
  bool periodic[3];
  periodicity(periodic);

  return ItNeighbor
    (this,min_face_rank,periodic,n3,index,
     neighbor_type,min_level,coarse_level);
}

//----------------------------------------------------------------------

Method * Block::method () throw ()
{
  Problem * problem = cello::problem();
  Method * method = problem->method(index_method_);
  return method;
}

//----------------------------------------------------------------------

Initial * Block::initial () throw ()
{
  Problem * problem = cello::problem();
  Initial * initial = problem->initial(index_initial_);
  return initial;
}

//----------------------------------------------------------------------

Solver * Block::solver () throw ()
{
  Problem * problem = cello::problem();
  Solver * solver = problem->solver(index_solver());
  return solver;
}

//----------------------------------------------------------------------

void Block::periodicity (bool p3[3]) const
{
  for (int axis=0; axis<3; axis++) {
    p3[axis] = false;
  }
  int index_boundary = 0;
  Problem * problem = cello::problem();
  Boundary * boundary;
  while ( (boundary = problem->boundary(index_boundary++)) ) {
    boundary->periodicity(p3);
  }
}

//----------------------------------------------------------------------

void Block::print () const
  
{
  CkPrintf ("data_ = %p\n",(void*)data_);
  CkPrintf ("child_data_ = %p\n",(void*)child_data_);
  int v3[3];index().values(v3);
  CkPrintf ("index_ = %0x %0x %0x\n",v3[0],v3[1],v3[2]);
  CkPrintf ("level_next_ = %d\n",level_next_);
  CkPrintf ("cycle_ = %d\n",cycle_);
  CkPrintf ("time_ = %f\n",time_);
  CkPrintf ("dt_ = %f\n",dt_);
  CkPrintf ("stop_ = %d\n",stop_);
  CkPrintf ("index_initial_ = %d\n",index_initial_);
  CkPrintf ("children_.size() = %lu\n",children_.size());
#ifdef OLD_ADAPT
  CkPrintf ("child_face_level_curr_.size() = %lu\n",child_face_level_curr_.size());
  CkPrintf ("child_face_level_next_.size() = %lu\n",child_face_level_next_.size());
#endif /* OLD_ADAPT */
  CkPrintf ("count_coarsen_ = %d\n",count_coarsen_);
  CkPrintf ("adapt_step_ = %d\n",adapt_step_);
  CkPrintf ("coarsened_ = %d\n",coarsened_);
  CkPrintf ("is_leaf_ = %d\n",is_leaf_);
  CkPrintf ("age_ = %d\n",age_);
  CkPrintf ("name_ = %s\n",name_.c_str());
  CkPrintf ("index_method_ = %d\n",index_method_);
  //  CkPrintf ("index_solver_ = %d\n",index_solver());
}

//=====================================================================

void Block::compute_derived(const std::vector< std::string>& field_list
                            /* = std::vector< std::string>() */ ) throw ()
/// @param      field_list     list of fields that may be derived to compute
{
  TRACE("Block::compute_derived()");

  // compute all derived fields on this block

  Field field = data()->field();

  int nderived = field.groups()->size("derived");

  if (nderived > 0){

    Problem * problem = cello::problem();
    Config   * config  = (Config *) cello::config();

    // this is not a good way of doing this...
    //   should contruct list of fields from full list
    //   rather than copying this loop twice...
    if (field_list.size() > 0){
      for (size_t i = 0; i < field_list.size(); i++){
        std::string name = field_list[i];
        if (field.groups()->is_in(name,"derived")){
          Compute * compute = problem->create_compute(name,
                                                      config);
          compute->compute(this);
          delete compute; // must be done
        }
      }
    } else{ // else check full field list and compute all
      // derived fields
      for(int i = 0; i < field.field_count(); i++){
        std::string name = field.field_name(i);
        if (field.groups()->is_in(name,"derived")){
          // call the appropriate compute object
          Compute * compute = problem->create_compute(name,
                                                      config);
          compute->compute(this);
          delete compute; // must be done
        }
      }
    }
  } // end if derived

  return;
}

//======================================================================

void Block::apply_initial_(MsgRefine * msg) throw ()
{

  bool is_first_cycle =  (cycle_ == cello::config()->initial_cycle);

  if (! is_first_cycle) {
    msg->update(data());
    delete msg;
  } else {
    delete msg;
    TRACE("Block::apply_initial_()");
    if (cello::config()->initial_new) {

      initial_new_begin_(0);

    } else {
      // Apply initial conditions

      index_initial_ = 0;
      Problem * problem = cello::problem();
      while (Initial * initial = problem->initial(index_initial_)) {
        initial->enforce_block(this,cello::hierarchy());
        index_initial_++;
      }

    } 

  }
}
//----------------------------------------------------------------------

Block::~Block()
{ 
  Simulation * simulation = cello::simulation();

#ifdef TRACE_BLOCK
  CkPrintf ("%d %s TRACE_BLOCK Block::~Block())\n",  CkMyPe(),name_.c_str());
#endif
#ifdef CELLO_DEBUG
  index_.print("~Block()",-1,2,false,simulation);
#endif

  
  Monitor * monitor = simulation ? simulation->monitor() : NULL;
  
  if (monitor && monitor->is_verbose()) {
    char buffer [80];
    sprintf (buffer,"~Block() %s (%d;%d;%d) destroyed",name().c_str(),
	     index_[0],index_[1],index_[2]);
    monitor->print("Adapt",buffer);
  }

  const int level = this->level();

  if (level > 0) {

    // Send restricted data to parent 

    int ic3[3];
    index_.child(level,ic3,ic3+1,ic3+2);

    int n; 
    char * array;
    int if3[3]={0,0,0};
    int g3[3]={0,0,0};
    Refresh * refresh = new Refresh;
    refresh->add_all_data();
    
    FieldFace * field_face = create_face
      ( if3,ic3,g3,refresh_coarse,refresh,true);
#ifdef DEBUG_FIELD_FACE  
    CkPrintf ("%d %s:%d DEBUG_FIELD_FACE creating %p\n",
              CkMyPe(),__FILE__,__LINE__,(void*)field_face);
#endif

    field_face->face_to_array(data()->field(),&n,&array);
    delete field_face;

    const Index index_parent = index_.index_parent();

    // --------------------------------------------------
    // ENTRY: #2 Block::~Block()-> Block::p_refresh_child()
    // ENTRY: parent if level > 0
    // --------------------------------------------------
    thisProxy[index_parent].p_refresh_child(n,array,ic3);
    // --------------------------------------------------

    delete [] array;
  }

  delete data_;
  data_ = 0;

  delete child_data_;
  child_data_ = 0;

  if (simulation) simulation->data_delete_block(this);

}

//----------------------------------------------------------------------

void Block::p_refresh_child 
(
 int    n, 
 char * buffer, 
 int    ic3[3]
 )
{
  performance_start_(perf_refresh_child);
  int if3[3] = {0,0,0};
  int  g3[3] = {0,0,0};
  Refresh * refresh = new Refresh;
  refresh->add_all_data();
  
  FieldFace * field_face = create_face
    (if3, ic3, g3, refresh_coarse,refresh,true);
#ifdef DEBUG_FIELD_FACE  
  CkPrintf ("%d %s:%d DEBUG_FIELD_FACE creating %p\n",
            CkMyPe(),__FILE__,__LINE__,(void*)field_face);
#endif
  
  field_face -> array_to_face (buffer, data()->field());
  delete field_face;
  performance_stop_(perf_refresh_child);
  performance_start_(perf_refresh_child_sync);
}

//----------------------------------------------------------------------

Block::Block ()
  : CBase_Block(),
    data_(NULL),
    child_data_(NULL),
    level_next_(0),
    cycle_(0),
    time_(0.0),
    dt_(0.0),
    stop_(false),
    index_initial_(0),
    children_(),
    sync_coarsen_(),
    sync_count_(),
    sync_max_(),
    adapt_(),
#ifdef OLD_ADAPT
    child_face_level_curr_(),
    child_face_level_next_(),
#endif /* OLD_ADAPT */
    count_coarsen_(0),
    adapt_step_(0),
    coarsened_(false),
    is_leaf_(true),
    age_(0),
    name_(""),
    index_method_(-1),
    index_solver_(),
    refresh_()
{
  init_refresh_();
  init_adapt_();
  
  for (int i=0; i<3; i++) array_[i]=0;
}


Block::Block (CkMigrateMessage *m)
  : CBase_Block(m),
    data_(NULL),
    child_data_(NULL),
    level_next_(0),
    cycle_(0),
    time_(0.0),
    dt_(0.0),
    stop_(false),
    index_initial_(0),
    children_(),
    sync_coarsen_(),
    sync_count_(),
    sync_max_(),
    adapt_(),
#ifdef OLD_ADAPT
    child_face_level_curr_(),
    child_face_level_next_(),
#endif /* OLD_ADAPT */
    count_coarsen_(0),
    adapt_step_(0),
    coarsened_(false),
    is_leaf_(true),
    age_(0),
    name_(""),
    index_method_(-1),
    index_solver_(),
    refresh_()
{
  init_refresh_();
  init_adapt_();

#ifdef TRACE_BLOCK
  CkPrintf ("TRACE_BLOCK Block(CkMigrateMessage*)\n");
  fflush(stdout);
#endif  
  
}

//----------------------------------------------------------------------

void Block::init_adapt_()
{
  if (index_.level() == 0) {
    const int rank = cello::rank();

    adapt_.set_rank(rank);
    const int ifmx = -1;
    const int ifpx = +1;
    const int ifmy = (rank >= 2) ? -1 : 0;
    const int ifpy = (rank >= 2) ? +1 : 0;
    const int ifmz = (rank >= 3) ? -1 : 0;
    const int ifpz = (rank >= 3) ? +1 : 0;
    int count = 0;
    int na3[3];
    cello::hierarchy()->root_blocks(na3,na3+1,na3+2);
    int if3[3];
    int k=0;
    int v3[3];
    index_.values(v3);
    adapt_.set_index(index_);
    for (if3[2]=ifmz; if3[2]<=ifpz; ++if3[2]) {
      for (if3[1]=ifmy; if3[1]<=ifpy; ++if3[1]) {
        for (if3[0]=ifmx; if3[0]<=ifpx; ++if3[0]) {
          if (if3[0] || if3[1] || if3[2]) {
            Index index_neighbor = index_.index_neighbor(if3,na3);
            index_neighbor.values(v3);
            adapt_.insert_neighbor(index_neighbor);
          }
          ++count;
        }
      }
    }
    count--; // don't count self
  }
}

//----------------------------------------------------------------------

void Block::init_refresh_()
{
  const int count = cello::simulation()->refresh_count();
  refresh_sync_list_.resize(count);
  refresh_msg_list_.resize(count);
  for (int i=0; i<count; i++) {
    refresh_sync_list_[i].reset();
  }
}

//----------------------------------------------------------------------

std::string Block::name() const throw()
{
  if (name_ == "") {

    name_ = name(index_);
  }
  return name_;
}

//----------------------------------------------------------------------

std::string Block::name(Index index) const throw()
{
  int blocking[3] = {1,1,1};
  cello::hierarchy()->root_blocks(blocking,blocking+1,blocking+2);
  
  const int level = index.level();
  for (int i=-1; i>=level; i--) {
    blocking[0] /= 2;
    blocking[1] /= 2;
    blocking[2] /= 2;
  }

  int bits[3] = {0,0,0};
    
  blocking[0]--;
  blocking[1]--;
  blocking[2]--;

  if (blocking[0]) do { ++bits[0]; } while (blocking[0]/=2);
  if (blocking[1]) do { ++bits[1]; } while (blocking[1]/=2);
  if (blocking[2]) do { ++bits[2]; } while (blocking[2]/=2);

  return std::string("B" + index.bit_string(level,cello::rank(),bits));
}

//----------------------------------------------------------------------

void Block::size_array (int * nx, int * ny, int * nz) const throw ()
{
  cello::hierarchy()->root_blocks(nx,ny,nz);
}

//----------------------------------------------------------------------

void Block::lower
(double * xm, double * ym, double * zm) const throw ()
{
  int  ix, iy, iz;
  int  nx, ny, nz;

  index_global (&ix,&iy,&iz,&nx,&ny,&nz);

  Hierarchy * hierarchy = cello::hierarchy();
  double xdm, ydm, zdm;
  hierarchy->lower(&xdm,&ydm,&zdm);
  double xdp, ydp, zdp;
  hierarchy->upper(&xdp,&ydp,&zdp);

  double ax = 1.0*ix/nx;
  double ay = 1.0*iy/ny;
  double az = 1.0*iz/nz;

  double xbm = (1.0-ax)*xdm + ax*xdp;
  double ybm = (1.0-ay)*ydm + ay*ydp;
  double zbm = (1.0-az)*zdm + az*zdp;

  if (xm) (*xm) = xbm;
  if (ym) (*ym) = ybm;
  if (zm) (*zm) = zbm;
}

//----------------------------------------------------------------------

void Block::upper
(double * xp, double * yp, double * zp) const throw ()
{
  int  ix, iy, iz;
  int  nx, ny, nz;

  index_global (&ix,&iy,&iz,&nx,&ny,&nz);

  Hierarchy * hierarchy = cello::hierarchy();
  double xdm, ydm, zdm;
  hierarchy->lower(&xdm,&ydm,&zdm);
  double xdp, ydp, zdp;
  hierarchy->upper(&xdp,&ydp,&zdp);

  double ax = 1.0*(ix+1)/nx;
  double ay = 1.0*(iy+1)/ny;
  double az = 1.0*(iz+1)/nz;

  double xbp = (1.0-ax)*xdm + ax*xdp;
  double ybp = (1.0-ay)*ydm + ay*ydp;
  double zbp = (1.0-az)*zdm + az*zdp;

  if (xp) (*xp) = xbp;
  if (yp) (*yp) = ybp;
  if (zp) (*zp) = zbp;
}

//----------------------------------------------------------------------

void Block::cell_width 
(double * dx, double * dy, double * dz) const throw()
{ 
  double xm,ym,zm;
  lower(&xm,&ym,&zm);
  double xp,yp,zp;
  upper(&xp,&yp,&zp);
  data()->field_data()->cell_width(xm,xp,dx, ym,yp,dy, zm,zp,dz);
}

//----------------------------------------------------------------------

void Block::index_global
( int *ix, int *iy, int *iz,
  int *nx, int *ny, int *nz ) const
{
  
  index_array(ix,iy,iz);
  size_array (nx,ny,nz);

  Index index = this->index();

  const int level = this->level();

  if (level < 0 ) {
    for (int i=level; i<0; i++) {
      if (ix) (*ix) = ((*ix) >> 1);
      if (iy) (*iy) = ((*iy) >> 1);
      if (iz) (*iz) = ((*iz) >> 1);
      if (nx) (*nx) >>= 1;
      if (ny) (*ny) >>= 1;
      if (nz) (*nz) >>= 1;
    }
  }  else if (0 < level) {
    for (int i=0; i<level; i++) {
      int bx,by,bz;
      index.child(i+1,&bx,&by,&bz);
      if (ix) (*ix) = ((*ix) << 1) | bx;
      if (iy) (*iy) = ((*iy) << 1) | by;
      if (iz) (*iz) = ((*iz) << 1) | bz;
      if (nx) (*nx) <<= 1;
      if (ny) (*ny) <<= 1;
      if (nz) (*nz) <<= 1;
    }
  }
}

//----------------------------------------------------------------------

FieldFace * Block::create_face
(int if3[3], int ic3[3], int g3[3],
 int refresh_type, Refresh * refresh, bool new_refresh) const
{
  FieldFace  * field_face = new FieldFace(cello::rank());
#ifdef DEBUG_FIELD_FACE  
  CkPrintf ("%d %s:%d DEBUG_FIELD_FACE creating %p\n",
            CkMyPe(),__FILE__,__LINE__,(void*)field_face);
#endif

  field_face -> set_refresh_type (refresh_type);
  field_face -> set_child (ic3[0],ic3[1],ic3[2]);
  field_face -> set_face (if3[0],if3[1],if3[2]);
  field_face -> set_ghost(g3[0],g3[1],g3[2]);
  field_face -> set_refresh(refresh,new_refresh);

  return field_face;
}

//----------------------------------------------------------------------

void Block::is_on_boundary (bool is_boundary[3][2]) const throw()
{

  // Boundary * boundary = simulation()->problem()->boundary();
  // bool periodic = boundary->is_periodic();

  int n3[3];
  size_array (&n3[0],&n3[1],&n3[2]);

  const int level = this->level();
  // adjust array size for negative levels
  if (level < 0) {
    int shift = -level;
    n3[0] = n3[0] >> shift;
    n3[1] = n3[1] >> shift;
    n3[2] = n3[2] >> shift;
  }

  for (int axis=0; axis<3; axis++) {
    for (int face=0; face<2; face++) {
      is_boundary[axis][face] = 
	index_.is_on_boundary(axis,2*face-1,n3[axis]);
    }
  }
}

//======================================================================

void Block::determine_boundary_
(
 bool bndry[3][2],
 bool * fxm, bool * fxp,
 bool * fym, bool * fyp,
 bool * fzm, bool * fzp
 )
{
  // return bndry[] array of faces on domain boundary

  is_on_boundary (bndry);

  int nx,ny,nz;
  data()->field_data()->size (&nx,&ny,&nz);

  // Determine in which directions we need to communicate or update boundary

  if (fxm && bndry[0][0]) *fxm = (nx > 1);
  if (fxp && bndry[0][1]) *fxp = (nx > 1);
  if (fym && bndry[1][0]) *fym = (ny > 1);
  if (fyp && bndry[1][1]) *fyp = (ny > 1);
  if (fzm && bndry[2][0]) *fzm = (nz > 1);
  if (fzp && bndry[2][1]) *fzp = (nz > 1);
}

//----------------------------------------------------------------------

void Block::update_boundary_ ()
{
  bool is_boundary[3][2];
  bool fxm=0,fxp=0,fym=0,fyp=0,fzm=0,fzp=0;

  determine_boundary_(is_boundary,&fxm,&fxp,&fym,&fyp,&fzm,&fzp);

  int index = 0;
  Problem * problem = cello::problem();
  Boundary * boundary;

  while ((boundary = problem->boundary(index++))) {
    // Update boundaries
    if ( fxm ) boundary->enforce(this,face_lower,axis_x);
    if ( fxp ) boundary->enforce(this,face_upper,axis_x);
    if ( fym ) boundary->enforce(this,face_lower,axis_y);
    if ( fyp ) boundary->enforce(this,face_upper,axis_y);
    if ( fzm ) boundary->enforce(this,face_lower,axis_z);
    if ( fzp ) boundary->enforce(this,face_upper,axis_z);
  }
}

//----------------------------------------------------------------------

void Block::facing_child_(int jc3[3], const int ic3[3], const int if3[3]) const
{
  jc3[0] = if3[0] ? 1 - ic3[0] : ic3[0];
  jc3[1] = if3[1] ? 1 - ic3[1] : ic3[1];
  jc3[2] = if3[2] ? 1 - ic3[2] : ic3[2];
  TRACE9("facing_child %d %d %d  child %d %d %d  face %d %d %d",
	 jc3[0],jc3[1],jc3[2],ic3[0],ic3[1],ic3[2],if3[0],if3[1],if3[2]);
}

//----------------------------------------------------------------------

void Block::copy_(const Block & block) throw()
{
  data_->copy_(*block.data());
  if (child_data_) child_data_->copy_(*block.child_data());

  cycle_      = block.cycle_;
  time_       = block.time_;
  dt_         = block.dt_;
  stop_       = block.stop_;
  adapt_step_ = block.adapt_step_;
  coarsened_  = block.coarsened_;
}

//----------------------------------------------------------------------

Index Block::neighbor_ 
(
 const int of3[3],
 Index *   ind
 ) const
{
  Index index = (ind != 0) ? (*ind) : index_;

  int na3[3];
  size_array (&na3[0],&na3[1],&na3[2]);
  // const bool periodic  = simulation()->problem()->boundary()->is_periodic();
  Index in = index.index_neighbor (of3,na3);
  return in;
}

//----------------------------------------------------------------------

void Block::performance_start_
(int index_region, std::string file, int line)
{
  Simulation * simulation = cello::simulation();
  if (simulation)
    simulation->performance()->start_region(index_region,file,line);
}

//----------------------------------------------------------------------

void Block::performance_stop_
(int index_region, std::string file, int line)
{
  Simulation * simulation = cello::simulation();
  if (simulation)
    simulation->performance()->stop_region(index_region,file,line);
}

//----------------------------------------------------------------------

void Block::check_leaf_()
{
  if (level() >= 0 &&
      ((  is_leaf() && children_.size() != 0) ||
       (! is_leaf() && children_.size() == 0))) {

    WARNING3("Block::check_leaf_()",
	     "%s: is_leaf() == %s && children_.size() == %lu",
	     name_.c_str(), is_leaf()?"true":"false",
	     children_.size());
  }
}


//----------------------------------------------------------------------

bool Block::check_position_in_block
(const double &x, const double &y, const double &z,
 bool include_ghost // default - false
 )
{

  double xm,ym,zm;
  lower(&xm,&ym,&zm);
  double xp,yp,zp;
  upper(&xp,&yp,&zp);

  bool result = false;

  if (include_ghost){
    int gx, gy, gz;
    double hx,hy,hz;
    data()->field().ghost_depth(0,&gx,&gy,&gz);
    cell_width(&hx,&hy,&hz);

    xm -= gx*hx;
    ym -= gy*hy;
    zm -= gz*hz;
    xp += gx*hx;
    yp += gy*hy;
    zp += gz*hz;
  }

  if (  ((x >= xm) && (x < xp)) &&
        ((y >= ym) && (y < yp)) &&
        ((z >= zm) && (z < zp))) result = true;

  return result;
}

//----------------------------------------------------------------------

void Block::debug_faces_(const char * mesg)
{
#ifdef DEBUG_ADAPT

#ifdef CELLO_DEBUG
  FILE * fp_debug = simulation()->fp_debug();
#endif
  TRACE_ADAPT(mesg);
  int if3[3] = {0};
  int ic3[3] = {0};

  for (ic3[1]=1; ic3[1]>=0; ic3[1]--) {
    for (if3[1]=1; if3[1]>=-1; if3[1]--) {

      PARALLEL_PRINTF ("%s ",name().c_str());

      for (if3[0]=-1; if3[0]<=1; if3[0]++) {
#ifdef CELLO_DEBUG
	fprintf (fp_debug,(ic3[1]==1) ? "%d " : "  ",face_level(if3));
#endif
	PARALLEL_PRINTF ((ic3[1]==1) ? "%d " : "  ",face_level(if3));
      }
#ifdef CELLO_DEBUG
      fprintf (fp_debug,"| ");
#endif
      PARALLEL_PRINTF ("| ") ;
      for (if3[0]=-1; if3[0]<=1; if3[0]++) {
#ifdef CELLO_DEBUG
	fprintf (fp_debug,(ic3[1]==1) ? "%d " : "  ",face_level_next(if3));
#endif
	PARALLEL_PRINTF ((ic3[1]==1) ? "%d " : "  ",face_level_next(if3));
      }
#ifdef CELLO_DEBUG
      fprintf (fp_debug,"| ");
#endif
      PARALLEL_PRINTF ("| ");
#ifdef OLD_ADAPT
      for (ic3[0]=0; ic3[0]<2; ic3[0]++) {
	for (if3[0]=-1; if3[0]<=1; if3[0]++) {
	  for (if3[0]=-1; if3[0]<=1; if3[0]++) {
#ifdef CELLO_DEBUG
	    fprintf (fp_debug,"%d ",child_face_level(ic3,if3));
#endif
	    PARALLEL_PRINTF ("%d ",child_face_level(ic3,if3));
	  }
	}
      }
#endif
#ifdef CELLO_DEBUG
      fprintf (fp_debug,"| ");
#endif
#ifdef OLD_ADAPT
      PARALLEL_PRINTF ("| ");
      for (ic3[0]=0; ic3[0]<2; ic3[0]++) {
	for (if3[0]=-1; if3[0]<=1; if3[0]++) {
	  for (if3[0]=-1; if3[0]<=1; if3[0]++) {
#ifdef CELLO_DEBUG
	    fprintf (fp_debug,"%d ",child_face_level_next(ic3,if3));
#endif
	    PARALLEL_PRINTF ("%d ",child_face_level_next(ic3,if3));
	  }
	}
      }
#endif
#ifdef CELLO_DEBUG
      fprintf (fp_debug,"\n");
      fflush(fp_debug);
#endif
      PARALLEL_PRINTF ("\n");
      fflush(stdout);

    }
  }
#endif
}

