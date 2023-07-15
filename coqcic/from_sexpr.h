#ifndef COQCIC_FROM_SEXPR_H
#define COQCIC_FROM_SEXPR_H

#include <variant>

#include "coqcic/constr.h"
#include "coqcic/parse_result.h"
#include "coqcic/sexpr.h"
#include "coqcic/sfb.h"

namespace coqcic {

struct from_sexpr_error {
	std::string description;
	const sexpr* context;
};

struct from_sexpr_str_error {
	std::string description;
	std::size_t location;
};

template<typename ResultType> using from_sexpr_result = parse_result<ResultType, from_sexpr_error>;
template<typename ResultType> using from_sexpr_str_result = parse_result<ResultType, from_sexpr_str_error>;

from_sexpr_result<constr_t>
constr_from_sexpr(const sexpr& e);

from_sexpr_result<sfb>
sfb_from_sexpr(const sexpr& e);

from_sexpr_str_result<constr_t>
constr_from_sexpr_str(const std::string& s);

from_sexpr_str_result<sfb>
sfb_from_sexpr_str(const std::string& s);


}  // namespace coqcic

#endif  // COQCIC_FROM_SEXPR_H
