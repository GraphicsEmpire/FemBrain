/*************************************************************************
 *                                                                       *
 * Vega FEM Simulation Library Version 1.0                               *
 *                                                                       *
 * "isotropic hyperelastic FEM" library , Copyright (C) 2012 USC         *
 * All rights reserved.                                                  *
 *                                                                       *
 * Code authors: Jernej Barbic, Fun Shing Sin                            *
 * http://www.jernejbarbic.com/code                                      *
 *                                                                       *
 * Research: Jernej Barbic, Fun Shing Sin, Daniel Schroeder,             *
 *           Doug L. James, Jovan Popovic                                *
 *                                                                       *
 * Funding: National Science Foundation, Link Foundation,                *
 *          Singapore-MIT GAMBIT Game Lab,                               *
 *          Zumberge Research and Innovation Fund at USC                 *
 *                                                                       *
 * Version 3.0                                                           *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of the BSD-style license that is            *
 * included with this library in the file LICENSE.txt                    *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the file     *
 * LICENSE.TXT for more details.                                         *
 *                                                                       *
 *************************************************************************/

#ifndef _HOMOGENEOUSNEOHOOKEANISOTROPICMATERIAL_H_
#define _HOMOGENEOUSNEOHOOKEANISOTROPICMATERIAL_H_

#include "isotropicMaterial.h"

/*
   Homogeneous neo-Hookean material. Material properties are constant throughout the mesh.

   The implemented neo-Hookean material is described in:
   BONET J., WOOD R. D.: Nonlinear Continuum Mechanics
   for Finite Element Analysis, 2nd Ed. Cambridge University
   Press, 2008, page 162
*/

class HomogeneousNeoHookeanIsotropicMaterial : public IsotropicMaterial
{
public:
  HomogeneousNeoHookeanIsotropicMaterial(double E, double nu);
  virtual ~HomogeneousNeoHookeanIsotropicMaterial();

  void SetYoungModulusAndPoissonRatio(double E, double nu);
  void SetLameCoefficients(double lambda, double mu);

  virtual double ComputeEnergy(int elementIndex, double * invariants);
  virtual void ComputeEnergyGradient(int elementIndex, double * invariants, double * gradient); // invariants and gradient are 3-vectors
  virtual void ComputeEnergyHessian(int elementIndex, double * invariants, double * hessian); // invariants is a 3-vector, hessian is a 3x3 symmetric matrix, unrolled into a 6-vector, in the following order: (11, 12, 13, 22, 23, 33).

protected:
  double E, nu;
  double lambdaLame, muLame; // Lam\'e coefficients, not "lame" :)
};

#endif

