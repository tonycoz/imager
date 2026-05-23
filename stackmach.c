#include "stackmach.h"

double
i_op_run(int codes[], size_t code_size, double parms[], size_t parm_size) {
  double stack[100];
  double *sp = stack;

  /* FIXME: add a function to validate the op codes agains the parms
     list size, and that the stack doesn't overflow or underflow and
     has only a single element at the end.
  */

  while (code_size) {
    switch (*codes++) {
    case bcAdd:
      sp[-2] += sp[-1];
      --sp;
      break;

    case bcSubtract:
      sp[-2] -= sp[-1];
      --sp;
      break;

    case bcDiv:
      sp[-2] /= sp[-1];
      --sp;
      break;

    case bcMult:
      sp[-2] *= sp[-1];
      --sp;
      break;

    case bcParm:
      if (code_size) {
        size_t index = *codes++;
        if (index >= parm_size)
          return 0.0;
        *sp++ = parms[index];
        --code_size;
      }
      else
        return 0.0;
      break;

    case bcSin:
      sp[-1] = sin(sp[-1]);
      break;
      
    case bcCos:
      sp[-1] = cos(sp[-1]);
      break;
      
    }
    --code_size;
  }
  
  return sp[-1];
}

