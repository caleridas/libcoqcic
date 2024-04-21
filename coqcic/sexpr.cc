#include "coqcic/sexpr.h"

#include <sstream>

namespace coqcic {

std::string
sexpr::debug_string() const {
	std::stringstream ss;
	format(ss);
	return ss.str();
}

sexpr_repr::~sexpr_repr() {
}

sexpr_terminal::~sexpr_terminal() {
}

void
sexpr_terminal::format(std::ostream& os) const {
	os << value();
}

std::unique_ptr<sexpr_repr>
sexpr_terminal::copy() const {
	return std::make_unique<sexpr_terminal>(value_, location());
}

sexpr_compound::~sexpr_compound() {
}

void
sexpr_compound::format(std::ostream& os) const {
	os << '(';
	os << kind_;
	for (const auto& arg : args()) {
		os << ' ';
		arg.format(os);
	}
	os << ')';
}

std::unique_ptr<sexpr_repr>
sexpr_compound::copy() const {
	return std::make_unique<sexpr_compound>(kind_, std::vector<sexpr>(args_.begin(), args_.end()), location());
}

}  // namespace coqcic
