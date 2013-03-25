#include "mex.h"
#include "ConstraintApp.h"

void mexFunction( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
  if ( nrhs != 1 ) {
    mexErrMsgTxt("requires exactly 1 parameters"); 
  }

  ConstraintApp* app(*(ConstraintApp**)mxGetData(prhs[0]));

  if ( nlhs != 1 ) {
    mexErrMsgTxt("returns exactly 1 parameters");
  }

  bool res = app->GetResetAndClear();

  plhs[0] = mxCreateLogicalScalar(res);
}
