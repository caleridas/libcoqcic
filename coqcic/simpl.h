#ifndef COQCIC_SIMPL_H
#define COQCIC_SIMPL_H

#include "coqcic/constr.h"

namespace coqcic {

// Substitute all occurrences of local variables starting at "index" with
// the expressions given by "subst". The substitutes themselves may contain local
// variable references which are assumed to be valid at the level of the
// expression just given (i.e. they will be shifted downwards if the substituion
// occurs at a deeper level).
constr
local_subst(
	const constr& input,
	std::size_t index,
	std::vector<constr> subst);

}  // namespace coqcic

#endif  // COQCIC_SIMPL_H
