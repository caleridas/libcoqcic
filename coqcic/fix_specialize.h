#ifndef COQCIC_FIX_SPECIALIZE_H
#define COQCIC_FIX_SPECIALIZE_H

#include "coqcic/constr.h"

namespace coqcic {

struct fix_spec_info {
	// Per-function specialization info.
	struct function {
		// Map "function argument number" to "specialization argument number".
		// The number of entries in this must match the number of parameters
		// that the corresponding fixpoint function is defined for.
		std::vector<std::optional<std::size_t>> spec_args;
	};

	// Specialization info for all functions in the group.
	std::vector<function> functions;
};

std::optional<fix_spec_info>
compute_fix_specialization_closure(
	const fix_group_t& group,
	std::size_t fn_index,
	std::vector<std::optional<std::size_t>> seed_arg);

fix_group_t
apply_fix_specialization(
	const fix_group_t& group,
	const fix_spec_info& info,
	const std::vector<constr_t>& spec_args,
	const std::function<std::string(std::size_t)>& namegen);

}  // namespace coqcic

#endif  // COQCIC_FIX_SPECIALIZE_H
