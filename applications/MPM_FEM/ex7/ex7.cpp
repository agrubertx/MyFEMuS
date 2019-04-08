#include "FemusInit.hpp"
#include "MultiLevelProblem.hpp"
#include "VTKWriter.hpp"
#include "TransientSystem.hpp"
#include "NonLinearImplicitSystem.hpp"
#include "Marker.hpp"
#include "Line.hpp"

#include "Fluid.hpp"
#include "Solid.hpp"
#include "Parameter.hpp"

#include "NumericVector.hpp"
#include "adept.h"

#include "../include/mpmFem.hpp"


#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
//double NeumannFactor = 0.;

using namespace femus;

// OLD BEST RESULT WITH E = 4.2 * 1.e6, 5 levels, dt= 0.01, NR = 300, R0 = 1.5, factor = 1.3
// MOST BEST RESULT WITH E = 4.2 * 1.e6, 4 levels, dt= 0.01, NR = 300, R0 = 1.4, factor = 1.14,  beta = 0.3, Gamma = 0.5

void ComputeIndexSet (std::vector < std::vector <unsigned> > & Jp,
                      const unsigned & degree, const unsigned & dimension, const bool &output = false);

void GaussianElemination (std::vector<std::vector < double > > & A, std::vector < double> &x, const bool &output = false);

void GetChebyshev (std::vector<double> &T, const unsigned &n, const double &x, const bool &output = false);

void GetMultiIndex (std::vector <unsigned> &idx, const unsigned &dim, const unsigned& n, const unsigned &i);
void SetElementDofs (std::vector <unsigned> &elementDofs, const std::vector < unsigned > & idx, const unsigned & nve1d);

int main (int argc, char** args) {


  bool output = true;

  // init Petsc-MPI communicator
  //FemusInit mpinit (argc, args, MPI_COMM_WORLD);

  std::vector < std::vector <unsigned> > aIdx;
  unsigned pOrder = 3;
  unsigned dim = 2;
  ComputeIndexSet (aIdx, pOrder, dim, output);

  unsigned nve1d = 5u;
  unsigned nve = static_cast< unsigned > (pow (nve1d, dim));
  unsigned nel1d = nve1d - 1u;
  unsigned nel = static_cast< unsigned > (pow (nel1d, dim));

  double Xv[5] = {0.,  0.1, 0.5, 1.,  1.3};
  double hv[5] = {0.1, 0.4, 0.5, 0.5, 0.3};

  std::vector < double > edgeSize (nel1d);
  for (unsigned iel = 0; iel < nel1d; iel++) {
    edgeSize[iel] = Xv[iel + 1] - Xv[iel];
  }

  unsigned  nDofs = static_cast < unsigned > ( pow(2, dim));

  std::vector < std::vector < unsigned > > elemDof (nel);

  std::vector <unsigned> elIdx (dim);
  for (unsigned iel = 0; iel < nel; iel++) {
    GetMultiIndex (elIdx, dim, nel1d, iel);
    SetElementDofs (elemDof[iel], elIdx, nve1d);
  }

  unsigned Np = aIdx.size();

  std::vector< std::vector < std::vector< double> > > Xp (nel);

  for (unsigned iel = 0; iel < nel; iel++) {
    GetMultiIndex (elIdx, dim, nel1d, iel);
    Xp[iel].resize (Np);
    if(elIdx[0] == 0 || elIdx[0] == nel1d - 1u) Xp[iel].resize (0);
    for (unsigned p = 0; p < Xp[iel].size(); p++) {
      Xp[iel][p].resize (dim);
      for (unsigned d = 0; d < dim; d++) {
        Xp[iel][p][d] = Xv[elIdx[d]] + (Xv[elIdx[d] + 1] - Xv[elIdx[d]]) * rand() / RAND_MAX;
      }
    }
  }

  std::vector < std::vector < std::vector< double> > > M (nve); // array of matrices
  for (unsigned i = 0; i < nve; i++) {
    M[i].resize (aIdx.size());
    for (unsigned k = 0; k < aIdx.size(); k++) {
      M[i][k].assign (aIdx.size() + 1, 0.);
    }
  }

  std::vector < std::vector < double > > T (dim);
 
  std::vector< double> nodeCounter (nve,0.);
  
  std::vector <unsigned> ndIdx (dim);
  for (unsigned iel = 0; iel < nel; iel++) {
    GetMultiIndex (elIdx, dim, nel1d, iel);
    for (unsigned p = 0; p < Xp[iel].size(); p++) {

      for (unsigned idof = 0; idof < nDofs; idof++) {
        unsigned i = elemDof[iel][idof];

        nodeCounter[i]++;
        
        GetMultiIndex (ndIdx, dim, nve1d, i);
        double W = 1.;

        for (unsigned d = 0 ; d < dim; d++) {
          GetChebyshev (T[d], pOrder, (Xv[ndIdx[d]] - Xp[iel][p][d]) / hv[ndIdx[d]]);
          W *= (1. - fabs (Xv[ndIdx[d]] - Xp[iel][p][d]) / edgeSize[elIdx[d]]);
        }
        for (unsigned k = 0; k < aIdx.size(); k++) {
          for (unsigned l = 0; l < aIdx.size(); l++) {
            double TkTl = 1;
            for (unsigned d = 0 ; d < dim; d++) {
              TkTl *= T[d][aIdx[k][d]] * T[d][aIdx[l][d]];
            }
            M[i][k][l] +=  W * TkTl;
          }
        }
      }
    }
  }

  
  std::vector < std::vector< double> > alpha (nve);
  GetChebyshev (T[0], pOrder, 0., false);
  for (unsigned i = 0; i < nve; i++) {
    if( nodeCounter[i] > aIdx.size() ) nodeCounter[i] = aIdx.size(); 
    
    std::cout << nodeCounter[i] <<" "<< aIdx.size() << std::endl;
    
    if( nodeCounter[i] > 0 ){
      M[i].resize (nodeCounter[i]);
      for (unsigned k = 0; k < nodeCounter[i]; k++) {
        M[i][k].resize (nodeCounter[i] + 1);
      }
      for (unsigned j = 0; j < nodeCounter[i]; j++) {
        double rhs = 1.;
        for (unsigned d = 0 ; d < dim; d++) {
          rhs *= T[0][ aIdx[j][d] ];
        }
        M[i][j][nodeCounter[i]] = rhs;
      }
      alpha[i].resize (nodeCounter[i]);
      GaussianElemination (M[i], alpha[i], false);
    }
  }
  
  std::vector < unsigned > pOrderTest(dim);
  for(unsigned d = 0; d < dim; d++){
    pOrderTest[d] = pOrder/dim;
  }
  pOrderTest[0] += pOrder%dim;
  
  std::cout << "Testing Polynomial \nPn = ";
  for (unsigned d = 0 ; d < dim; d++) {
    std::cout<<"x"<<d<<"^"<<pOrderTest[d]<<" * ";
  }
  std::cout<<"\b\b  "<<std::endl;
  
  std::vector < double > Ur (nve, 0.);
  for (unsigned iel = 0; iel < nel; iel++) {
    GetMultiIndex (elIdx, dim, nel1d, iel);
    for (unsigned p = 0; p < Xp[iel].size(); p++) {

      for (unsigned idof = 0; idof < nDofs; idof++) {
        unsigned i = elemDof[iel][idof];

        if( nodeCounter[i] > 0 ){
        
          GetMultiIndex (ndIdx, dim, nve1d, i);
        
          double W = 1.;
          double P = 1.;
          for (unsigned d = 0 ; d < dim; d++) {
            GetChebyshev (T[d], pOrder, (Xv[ndIdx[d]] - Xp[iel][p][d]) / hv[ndIdx[d]]);
            W *= (1. - fabs (Xv[ndIdx[d]] - Xp[iel][p][d]) / edgeSize[elIdx[d]]);
            P *= pow (Xp[iel][p][d], pOrderTest[d]);
          }

          double sumAlphaT = 0.;
          for (unsigned k = 0; k < nodeCounter[i]; k++) {
            double Tk = 1;
            for (unsigned d = 0 ; d < dim; d++) {
              Tk *= T[d][aIdx[k][d]];
            }
            sumAlphaT += alpha[i][k] * Tk;
          }
          Ur[i] += W * sumAlphaT  * P;
        }
      }
    }
  }
  
  bool test_passed = true;
  for (unsigned i = 0; i < nve; i++) {
    if(nodeCounter[i] > 0){
      GetMultiIndex (ndIdx, dim, nve1d, i);
      double P = 1.;
      for (unsigned d = 0 ; d < dim; d++) {
        P *= pow (Xv[ndIdx[d]], pOrderTest[d]);
      }
      if( fabs(P - Ur[i]) > 1.0e-6){
        test_passed = false;
        std::cout <<"Error at node = "<<i <<" exact value = " << P << " reconstructed value = " << Ur[i] << std::endl;
      }
    }
  }
  if(test_passed == true) std::cout << "Test passed";
  std::cout << std::endl;

}


void GaussianElemination (std::vector<std::vector < double > > & A, std::vector < double> &x, const bool &output) {

  unsigned n = A.size();

  if (output) {
    std::cout << "Before LU\n";
    for (unsigned i = 0; i < n; i++) {
      for (unsigned j = 0; j < n + 1; j++) {
        std::cout << A[i][j] << " ";
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }

  for (unsigned i = 0; i < n - 1; i++) {
    unsigned p = i;
    while (A[p][i] == 0) {
      p++;
      if (p == n) {
        std::cout << "The Matrix A is singular\n";
        exit (0);
      }
    }
    if (p != i) {
      for (unsigned j = 0; j < n + 1; j++) {
        double tmp;
        tmp = A[i][j];
        A[i][j] = A[p][j];
        A[p][j] = tmp;
      }
    }
    for (unsigned j = i + 1; j < n; j++) {
      double mji = A[j][i] / A[i][i];
      for (unsigned k = i; k < n + 1; k++) {
        A[j][k] -= mji * A[i][k];
      }
    }
  }
  if (A[n - 1][n - 1] == 0) {
    std::cout << "The Matrix A is singular\n";
    exit (0);
  }
  else {
    x[n - 1] = A[n - 1][n] / A[n - 1][n - 1];
    for (int i = n - 2; i >= 0; i--) {
      x[i] = A[i][n];
      for (unsigned j = i + 1; j < n; j++) {
        x[i] -= A[i][j] * x[j];
      }
      x[i] /= A[i][i];
    }
  }

  if (output) {
    std::cout << "After LU\n";
    for (unsigned i = 0; i < n; i++) {
      for (unsigned j = 0; j < n; j++) {
        std::cout << A[i][j] << " ";
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }

  return;
}

void ComputeIndexSet (std::vector < std::vector <unsigned> > & Jp,
                      const unsigned & degree, const unsigned & dimension, const bool &output) { //p is max poly degree


  unsigned dimJp = static_cast <unsigned> (boost::math::binomial_coefficient<double> (dimension + degree, degree));

  Jp.resize (dimJp);
  for (unsigned i = 0; i < dimJp; i++) {
    Jp[i].resize (dimension);
  }

  unsigned index = 0;
  unsigned counters[dimension + 1];
  memset (counters, 0, sizeof (counters));

  while (!counters[dimension]) {

    unsigned entrySum = 0;
    for (unsigned j = 0; j < dimension; j++) {
      entrySum += counters[j];
    }

    if (entrySum <= degree) {
      for (unsigned j = 0; j < dimension; j++) {
        Jp[index][j] = counters[dimension - 1 - j];
        if (output) {
          std::cout << "alpha[" << index << "][" << j << "]= " << Jp[index][j] << " ";
        }
      }
      if (output) {
        std::cout << std::endl;
      }
      index++;
    }
    unsigned i;
    for (i = 0; counters[i] == degree; i++) {   // inner loops that are at maxval restart at zero
      counters[i] = 0;
    }
    ++counters[i];  // the innermost loop that isn't yet at maxval, advances by 1
  }
  if (output) {
    std::cout << std::endl;
  }
}


void GetChebyshev (std::vector<double> &T, const unsigned &n, const double &x, const bool &output) {
  T.resize (n + 1);
  T[0] = 1;
  T[1] = x;
  for (unsigned i = 2; i < n + 1; i++) {
    T[i] = 2 * x * T[ i - 1] - T[ i - 2];
  }
  if (output) {
    std::cout << "Chebyshev Polynomilas at x = " << x << std::endl;
    for (unsigned i = 0; i < n + 1; i++) {
      std::cout << "T" << i << " [x] = " << T[i] << std::endl;
    }
    std::cout << std::endl;

  }

}

void GetMultiIndex (std::vector <unsigned> &idx, const unsigned &dim, const unsigned& n, const unsigned &i) {
  idx.resize (dim);
  for (unsigned d = 0; d < dim; d++) {
    idx[d] = (i % static_cast < unsigned > (pow (n, dim - d))) / static_cast < unsigned > (pow (n, dim - 1 - d));
  }
}


void SetElementDofs (std::vector <unsigned> &elementDof, const std::vector < unsigned > & idx, const unsigned & nve1d) {

  unsigned dim = idx.size();
  unsigned nDofs = static_cast < unsigned > (pow( 2, dim ) );  
  
  elementDof.assign (nDofs, 0);
  unsigned sizeHalf = nDofs / 2u;
  
  unsigned jj;
  for(unsigned d = 0; d < dim; d++){
    for(unsigned j = 0; j < nDofs; j++){
      jj = j;  
      while ( jj >= (2u * sizeHalf) ) {
        jj -= 2u * sizeHalf; 
      }
      jj /= sizeHalf;
         
      elementDof[j] += ( idx[d] + jj ) * ( static_cast <unsigned> ( pow( nve1d , dim - 1u - d)) );
      
    }
    sizeHalf /= 2;
  }  
}