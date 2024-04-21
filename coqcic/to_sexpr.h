#ifndef COQCIC_TO_SEXPR_H
#define COQCIC_TO_SEXPR_H

#include "coqcic/constr.h"
#include "coqcic/parse_result.h"
#include "coqcic/sexpr.h"
#include "coqcic/sfb.h"

namespace coqcic {

sexpr constr_to_sexpr(const constr_t& constr);

}  // namespace coqcic

#endif  // COQCIC_TO_SEXPR_H
