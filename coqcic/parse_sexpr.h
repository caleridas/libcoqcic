#ifndef COQCIC_PARSE_SEXPR_H
#define COQCIC_PARSE_SEXPR_H

#include <istream>
#include <string>

#include "coqcic/parse_result.h"
#include "coqcic/sexpr.h"

namespace coqcic {

struct sexpr_parse_error {
	std::string description;
	// character index into source
	std::size_t location;
};

template<typename ResultType>
using sexpr_parse_result = parse_result<ResultType, sexpr_parse_error>;

sexpr_parse_result<sexpr>
parse_sexpr(std::istream& s);

sexpr_parse_result<sexpr>
parse_sexpr(const std::string& s);


}  // namespace coqcic

#endif  // COQCIC_PARSE_SEXPR_H
