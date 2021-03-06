/*************************************************************************
 *                                                                       *
 * Vega FEM Simulation Library Version 1.0                               *
 *                                                                       *
 * "sparseMatrixMT" library , Copyright (C) 2007 CMU, 2009 MIT, 2012 USC *
 * All rights reserved.                                                  *
 *                                                                       *
 * Code author: Jernej Barbic                                            *
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

#ifndef _SPARSE_MATRIX_MT_H_
#define _SPARSE_MATRIX_MT_H_

/*
  Multithreaded version of the sparse matrix library. Performs matrix-vector multiplications in parallel, using OpenMP.
  To use it, you should enable the USE_OPENMP flag in sparseMatrixMT.cpp, and compile the code with the flag -fopenmp.
*/

#include "sparseMatrix.h"

class SparseMatrixMT
{
public:

  // multiplies the sparse matrix with the given vector
  static void MultiplyVector(const SparseMatrix * A, const double * input, double * result, int numThreads=-1); // result = A * input
};

#endif

