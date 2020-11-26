%module gkhtm
%{
#include "HTMCircleRegion.h"
#include "HTMCircleAllIDsCassandra.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <std_pair.i>

namespace std {
   %template(VectorULongLong) vector<unsigned long long>;
   %template(VectorString) vector<string>;
   %template(PairDoubleDouble) pair<double,double>;
   %template(VectorPairDoubleDouble) vector<pair<double,double> >;
}

// Include the header file with above prototypes
%include "HTMCircleRegion.h"

%include "HTMCircleAllIDsCassandra.h"
