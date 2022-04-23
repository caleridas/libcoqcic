#ifndef COQCIC_NORMALIZE_H
#define COQCIC_NORMALIZE_H

#include "coqcic/constr.h"

namespace coqcic {

// Normalizes the given expression, that means:
// - "apply-of-apply" will be flattened into a single apply
// - "product-of-product" will be flattened into a single product
// - "lambda-of-lambda" will be flattened into a single lambda
constr
normalize(const constr& expr);

}  // namespace coqcic

#endif  // COQCIC_NORMALIZE_H
