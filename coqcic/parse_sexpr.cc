#include "coqcic/parse_sexpr.h"

#include <sstream>

namespace coqcic {

namespace {

bool
is_whitespace(char c) {
	return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

bool
is_normal_char(char c) {
	return !is_whitespace(c) && c != '(' && c != ')' && c != '"' && c != 0;
}

class parser {
public:
	inline explicit
	parser(std::istream& stream) : stream_(stream), index_(0) {
		stream_.get(current_);
		if (stream_.eof()) {
			current_ = 0;
		}
	}

	sexpr_parse_result<sexpr>
	parse_terminal();

	sexpr_parse_result<sexpr>
	parse_compound();

	sexpr_parse_result<sexpr>
	parse_expr();

	inline void
	skip_whitespace() {
		while (is_whitespace(current_)) {
			next();
		}
	}

private:
	inline void
	next() {
		++index_;
		stream_.get(current_);
		if (stream_.eof()) {
			current_ = 0;
		}
	}

	std::istream& stream_;
	char current_;
	std::size_t index_;
};

sexpr_parse_result<sexpr>
parser::parse_terminal() {
	std::size_t location = index_;

	std::string value;

	while (is_normal_char(current_)) {
		value += current_;
		next();
	}

	if (value.empty()) {
		return sexpr_parse_error{"Empty or invalid terminal", index_};
	}

	skip_whitespace();

	return sexpr::make_terminal(std::move(value), location);
}

sexpr_parse_result<sexpr>
parser::parse_compound() {
	std::size_t location = index_;

	std::string kind;
	std::vector<sexpr> args;

	next();
	skip_whitespace();
	while (is_normal_char(current_)) {
		kind += current_;
		next();
	}

	if (kind.empty()) {
		return sexpr_parse_error{"Empty or invalid compound kind", index_};
	}

	skip_whitespace();

	while (current_ != 0 && current_ != ')') {
		auto sub = parse_expr();
		if (!sub) {
			return sub.error();
		}
		args.push_back(sub.move_value());
	}
	if (current_ == 0) {
		return sexpr_parse_error{"Unexpected end of stream", index_};
	}

	next();
	skip_whitespace();

	return sexpr::make_compound(std::move(kind), std::move(args), location);
}

sexpr_parse_result<sexpr>
parser::parse_expr() {
	if (current_ == '(') {
		return parse_compound();
	} else {
		return parse_terminal();
	}
}

}  // namespace

sexpr_parse_result<sexpr>
parse_sexpr(std::istream& s) {
	parser p(s);
	p.skip_whitespace();
	return p.parse_expr();
}

sexpr_parse_result<sexpr>
parse_sexpr(const std::string& s) {
	std::stringstream ss(s, std::ios_base::in);
	return parse_sexpr(ss);
}

}  // namespace coqcic
