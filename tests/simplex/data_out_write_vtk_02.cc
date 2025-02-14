// ---------------------------------------------------------------------
//
// Copyright (C) 2020 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------



// Test DataOut::write_vtk() for simplex meshes.

#include <deal.II/base/quadrature_lib.h>

#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_pyramid_p.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_simplex_p.h>
#include <deal.II/fe/fe_simplex_p_bubbles.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_wedge_p.h>
#include <deal.II/fe/mapping_fe.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/tria.h>

#include <deal.II/numerics/data_out.h>
#include <deal.II/numerics/vector_tools.h>

#include "../tests.h"

#include "./simplex_grids.h"

using namespace dealii;

template <int dim>
class RightHandSideFunction : public Function<dim>
{
public:
  RightHandSideFunction(const unsigned int n_components)
    : Function<dim>(n_components)
  {}

  virtual double
  value(const Point<dim> &p, const unsigned int component = 0) const
  {
    return p[component % dim] * p[component % dim];
  }
};

template <int dim, int spacedim = dim>
void
test(const FiniteElement<dim, spacedim> &fe_0,
     const FiniteElement<dim, spacedim> &fe_1,
     const unsigned int                  n_components,
     const bool                          do_high_order)
{
  const unsigned int degree = fe_0.tensor_degree();

  hp::FECollection<dim, spacedim> fe(fe_0, fe_1);

  hp::QCollection<dim> quadrature(QGaussSimplex<dim>(degree + 1),
                                  QGauss<dim>(degree + 1));

  hp::MappingCollection<dim, spacedim> mapping(
    MappingFE<dim, spacedim>(FE_SimplexP<dim, spacedim>(1)),
    MappingQGeneric<dim, spacedim>(1));

  Triangulation<dim, spacedim> tria;
  GridGenerator::subdivided_hyper_cube_with_simplices_mix(tria,
                                                          dim == 2 ? 4 : 2);

  DoFHandler<dim> dof_handler(tria);

  for (const auto &cell : dof_handler.active_cell_iterators())
    {
      if (cell->is_locally_owned() == false)
        continue;

      if (cell->reference_cell() == ReferenceCells::Triangle)
        cell->set_active_fe_index(0);
      else if (cell->reference_cell() == ReferenceCells::Quadrilateral)
        cell->set_active_fe_index(1);
      else
        Assert(false, ExcNotImplemented());
    }

  dof_handler.distribute_dofs(fe);

  Vector<double> solution(dof_handler.n_dofs());

  AffineConstraints<double> dummy;
  dummy.close();

  VectorTools::project(mapping,
                       dof_handler,
                       dummy,
                       quadrature,
                       RightHandSideFunction<dim>(n_components),
                       solution);

  static unsigned int counter = 0;

  for (unsigned int n_subdivisions = 1;
       n_subdivisions <= (do_high_order ? 4 : 2);
       ++n_subdivisions)
    {
      DataOutBase::VtkFlags flags;
      flags.write_higher_order_cells = do_high_order;

      DataOut<dim> data_out;
      data_out.set_flags(flags);

      data_out.attach_dof_handler(dof_handler);
      data_out.add_data_vector(solution, "solution");


      data_out.build_patches(mapping, n_subdivisions);

#if false
      std::ofstream output("test." + std::to_string(dim) + "." +
                           std::to_string(counter++) + ".vtk");
      data_out.write_vtk(output);
#else
      data_out.write_vtk(deallog.get_file_stream());
#endif
    }
}

int
main()
{
  initlog();

  for (unsigned int i = 0; i < 2; ++i)
    {
      const bool do_high_order = (i == 1);

      if (true)
        {
          const unsigned int dim = 2;
          test<dim>(FE_SimplexP<dim>(2), FE_Q<dim>(2), 1, do_high_order);

          test<dim>(FESystem<dim>(FE_SimplexP<dim>(2), dim),
                    FESystem<dim>(FE_Q<dim>(2), dim),
                    dim,
                    do_high_order);

          test<dim>(
            FESystem<dim>(FE_SimplexP<dim>(2), dim, FE_SimplexP<dim>(1), 1),
            FESystem<dim>(FE_Q<dim>(2), dim, FE_Q<dim>(1), 1),
            dim + 1,
            do_high_order);
        }
    }
}
