// Copyright (C) 2004-2011 Jed Brown, Ed Bueler and Constantine Khroulev
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

/*! \page computational_grid Organization of PISM's computational grid

  PISM uses the class IceGrid to manage computational grids and their
  parameters.

  Computational grids PISM can use are
  - rectangular,
  - equally spaced in the horizintal (X and Y) directions,
  - distributed across processors in horizontal dimensions only (every column
    is stored on one processor only),
  - are periodic in both X and Y directions (in the topological sence).

  Each processor "owns" a rectangular patch of xm times ym grid points with
  indices starting at xs and ys in the X and Y directions respectively.

  The typical code performing a point-wise computation will look like

  \code
  for (PetscInt i=grid.xs; i<grid.xs+grid.xm; ++i) {
    for (PetscInt j=grid.ys; j<grid.ys+grid.ym; ++j) {
    // compute something at i,j
    }
  }
  \endcode

  For finite difference (and some other) computations we often need to know
  values at map-plane neighbors of a grid point. 

  We say that a patch owned by a processor is surrounded by a strip of "ghost"
  grid points belonging to patches next to the one in question. This lets us to
  access (read) values at all the eight neighbors of a grid point for \e all
  the grid points, including ones at an edge of a processor patch \e and at an
  edge of a computational domain.

  All the values \e written to ghost points will be lost next time ghost values
  are updated.

  Sometimes it is beneficial to update ghost values locally (for instance when
  a computation A uses finite differences to compute derivatives of a quantity
  produced using a purely local (point-wise) computation B). In this case the
  double loop above can be modified to look like

  \code
  int GHOSTS = 1;
  for (PetscInt i=grid.xs - GHOSTS; i<grid.xs+grid.xm + GHOSTS; ++i) {
    for (PetscInt j=grid.ys - GHOSTS; j<grid.ys+grid.ym + GHOSTS; ++j) {
    // compute something at i,j
    }
  }
  \endcode

*/

#ifndef __grid_hh
#define __grid_hh

#include <petscda.h>
#include "NCVariable.hh"
#include "PISMProf.hh"

typedef enum {UNKNOWN = 0, EQUAL, QUADRATIC} SpacingType;
typedef enum {NONE = 0, X_PERIODIC = 1, Y_PERIODIC = 2, XY_PERIODIC = 3} Periodicity;

//! Describes the PISM grid and the distribution of data across processors.
/*!
  This class holds parameters describing the grid, including the vertical
  spacing and which part of the horizontal grid is owned by the processor. It
  contains the dimensions of the PISM (4-dimensional, x*y*z*time) computational
  box. The vertical spacing can be quite arbitrary.

  It creates and destroys a two dimensional \c PETSc \c DA (distributed array).
  The creation of this \c DA is the point at which PISM gets distributed across
  multiple processors.

  It computes grid parameters for the fine and equally-spaced vertical grid
  used in the conservation of energy and age equations.
 */
class IceGrid {
public:
  IceGrid(MPI_Comm c, PetscMPIInt r, PetscMPIInt s, const NCConfigVariable &config);
  ~IceGrid();

  PetscErrorCode report_parameters();

  PetscErrorCode createDA();  // destructor checks if DA was created, and destroys
  PetscErrorCode createDA(PetscInt procs_x, PetscInt procs_y,
			  PetscInt* &lx, PetscInt* &ly);
  PetscErrorCode set_vertical_levels(int Mz, int Mbz, double *z_levels, double *zb_levels);
  PetscErrorCode compute_vertical_levels();
  PetscErrorCode compute_ice_vertical_levels();
  PetscErrorCode compute_bed_vertical_levels();
  PetscErrorCode compute_horizontal_spacing();
  void compute_point_neighbors(PetscReal x, PetscReal y,
                               PetscInt &i, PetscInt &j);
  vector<PetscReal> compute_interp_weights(PetscReal x, PetscReal y);

  void compute_nprocs();
  void compute_ownership_ranges();
  PetscErrorCode compute_viewer_size(int target, int &x, int &y);
  PetscErrorCode printInfo(int verbosity); 
  PetscErrorCode printVertLevels(int verbosity); 
  PetscInt       kBelowHeight(PetscScalar height);
  void mapcoords(PetscInt i, PetscInt j,
                 PetscScalar &x, PetscScalar &y, PetscScalar &r);
  PetscErrorCode create_viewer(PetscInt viewer_size, string title, PetscViewer &viewer);

  MPI_Comm    com;
  PetscMPIInt rank, size;
  DA          da2;		// whether this is PETSC_NULL is important;
				// some functions use it to determine if values
				// in this IceGrid instance can be trusted
  PetscInt    xs, xm, ys, ym;

  PetscScalar *zlevels, *zblevels; // z levels, in ice & bedrock; the storage grid for fields 
                                   // which are represented in 3d Vecs
  vector<PetscReal> x, y;          // grid coordinates

  // Fine vertical grid and the interpolation setup:
  PetscScalar *zlevels_fine, *zblevels_fine, dz_fine;
  PetscInt    Mz_fine, Mbz_fine;
  // Array ice_storage2fine contains indices of the ice storage vertical grid
  // that are just below a level of the fine grid. I.e. ice_storage2fine[k] is
  // the storage grid level just below fine-grid level k (zlevels_fine[k]).
  // Similarly for other arrays below.
  PetscInt *ice_storage2fine, *ice_fine2storage,
    *bed_storage2fine, *bed_fine2storage;

  SpacingType ice_vertical_spacing, bed_vertical_spacing;
  Periodicity periodicity;
  PetscScalar dzMIN, dzMAX;
  PetscScalar dzbMIN, dzbMAX;

  PetscScalar x0, y0;	   // grid center (from an input or bootstrapping file)
  PetscScalar Lx, Ly; // half width of the ice model grid in x-direction, y-direction (m)
  PetscInt    Mx, My; // number of grid points in the x-direction, y-direction
  PetscInt    Nx, Ny; // number of processors in the x-direcion, y-direction
  PetscInt    *procs_x, *procs_y;
  PetscScalar dx, dy; // horizontal grid spacing

  PetscScalar Lz, Lbz; // extent of the ice, bedrock in z-direction (m)
  PetscInt    Mz, Mbz; // number of grid points in z-direction, ice and bedrock.
  PetscInt initial_Mz; // initial number of grid levels; used by the grid extension code
  PetscInt max_stencil_width;   // maximum stancil width supported by the DA in
                                // this IceGrid object

  PetscScalar year,       //!< current time (years)
    start_year,		  //!< the year this run started from
    end_year;		  //!< time to stop at
  
  PISMProf *profiler;
protected:
  PetscScalar lambda;	 // vertical spacing parameter
  PetscErrorCode get_dzMIN_dzMAX_spacingtype();
  PetscErrorCode compute_horizontal_coordinates();
  PetscErrorCode compute_fine_vertical_grid();
  PetscErrorCode init_interpolation();
};

#endif	/* __grid_hh */
