#ifndef COQCIC_NORMALIZE_H
#define COQCIC_NORMALIZE_H

#include "coqcic/constr.h"

namespace coqcic {

/**
	\brief Normalizes the given expression

	\param expr
		Expression to be normalized

	\returns
		Normalized form of original expression.

	Normalizes the expression by ensuring that:
	- "apply-of-apply" will be flattened into a single apply
	- "product-of-product" will be flattened into a single product
	- "lambda-of-lambda" will be flattened into a single lambda
*/
constr_t
normalize(const constr_t& expr);

}  // namespace coqcic

#endif  // COQCIC_NORMALIZE_H
