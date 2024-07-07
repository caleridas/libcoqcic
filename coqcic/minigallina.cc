#include "coqcic/minigallina.h"

#include <sstream>

#include <iostream>

#include "coqcic/normalize.h"

namespace coqcic {
namespace mgl {

namespace {

bool is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_digit(char c) {
	return c >= '0' && c <= '9';
}

bool is_identifier_start(char c) {
	return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_identifier_cont(char c) {
	return is_identifier_start(c) || is_digit(c);
}

struct keyword_entry_t {
	keyword_t code;
	const char str[20];
};

static keyword_entry_t keyword_mapping[] = {
	{keyword_definition, "Definition"},
	{keyword_fixpoint, "Fixpoint"},
	{keyword_inductive, "Inductive"},
	{keyword_module, "Module"},
	{keyword_End, "End"},
	{keyword_end, "end"},
	{keyword_match, "match"},
	{keyword_with, "with"},
	{keyword_forall, "forall"},
	{keyword_let, "let"},
	{keyword_in, "in"},
	{keyword_as, "as"},
	{keyword_return, "return"},
	{keyword_fun, "fun"}
};

std::string
keyword_name(keyword_t kw) {
	for (const auto& entry : keyword_mapping) {
		if (entry.code == kw) {
			return entry.str;
		}
	}
	throw std::logic_error("unhandled keyword enum");
}

struct symbol_entry_t {
	symbol_t code;
	const char str[20];
};

static symbol_entry_t symbol_mapping[] = {
	{symbol_dot, "."},
	{symbol_comma, ","},
	{symbol_colon, ":"},
	{symbol_equals, "="},
	{symbol_assign, ":="},
	{symbol_open_paren, "("},
	{symbol_close_paren, ")"},
	{symbol_arrow, "->"},
	{symbol_mapsto, "=>"},
	{symbol_pipe, "|"}
};

std::optional<symbol_t> lookup_symbol(const std::string& s) {
	for (const auto& entry : symbol_mapping) {
		if (s == entry.str) {
			return entry.code;
		}
	}

	return std::nullopt;
}

std::string
symbol_name(symbol_t symbol) {
	for (const auto& entry : symbol_mapping) {
		if (entry.code == symbol) {
			return entry.str;
		}
	}
	throw std::logic_error("unhandled symbol enum");
}

type_context_t make_type_context(
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve
) {
	type_context_t ctx = {
		locals_types,
		[&globals_resolve](const std::string& s)
		{
			auto r = globals_resolve(s);
			if (r) {
				return *r;
			} else {
				throw std::runtime_error("unresolved global: " + s);
			}
		}
	};

	return ctx;
}

const constructor_t* lookup_constructor(const one_inductive_t& ind, const std::string& name) {
	for (const auto& cons : ind.constructors) {
		if (cons.id == name) {
			return &cons;
		}
	}
	return nullptr;
}

std::vector<constr_t> get_constructor_arguments(const constructor_t& cons) {
	std::vector<constr_t> args;
	if (auto prod = cons.type.as_product()) {
		for (const auto& arg : prod->args()) {
			args.push_back(arg.type);
		}
	}

	return args;
}

bool token_is_symbol(const token_t& token, symbol_t symbol) {
	return std::visit(
		[symbol](const auto& arg)
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same<T, token_symbol>()) {
				return arg.symbol == symbol;
			}
			return false;
		},
		token);
}

bool token_is_keyword(const token_t& token, keyword_t keyword) {
	return std::visit(
		[keyword](const auto& arg)
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same<T, token_keyword>()) {
				return arg.keyword == keyword;
			}
			return false;
		},
		token);
}


}  // namespace

constr_ast_node::~constr_ast_node() {
}

parse_result<formal_arg_t, parse_error>
constr_ast_formarg::resolve(
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) const {
	auto resolved_type = type->resolve(locals_map, locals_types, globals_resolve, inductive_resolve);
	if (!resolved_type) {
		return resolved_type.error();
	}
	return formal_arg_t { id, resolved_type.move_value() };
}

constr_ast_node_id::~constr_ast_node_id() {
}

parse_result<constr_t, parse_error>
constr_ast_node_id::resolve(
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) const {
	if (id() == "Set") {
		return builder::builtin_set();
	}
	if (id() == "Prop") {
		return builder::builtin_prop();
	}
	if (id() == "Type") {
		return builder::builtin_type();
	}
	auto i = locals_map.get_index(id());
	if (i) {
		return builder::local(id(), *i);
	}

	auto g = globals_resolve(id());
	if (g) {
		return builder::global(id());
	}

	return parse_error { "Cannot resolve name '" + id() + "'", location() };
}

constr_ast_node_apply::~constr_ast_node_apply() {
}

parse_result<constr_t, parse_error>
constr_ast_node_apply::resolve(
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) const {
	auto res = fn_->resolve(locals_map, locals_types, globals_resolve, inductive_resolve);
	if (!res) {
		return res.error();
	}

	constr_t fn = res.move_value();
	std::vector<constr_t> args;
	for (const auto& arg : args_) {
		auto res = arg->resolve(locals_map, locals_types, globals_resolve, inductive_resolve);
		if (!res) {
			return res.error();
		}
		args.push_back(res.move_value());
	}

	return builder::apply(std::move(fn), std::move(args));
}

constr_ast_node_let::~constr_ast_node_let() {
}

parse_result<constr_t, parse_error>
constr_ast_node_let::resolve(
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) const {
	auto value = value_->resolve(locals_map, locals_types, globals_resolve, inductive_resolve);
	if (!value) {
		return value.error();
	}

	auto type = type_->resolve(locals_map, locals_types, globals_resolve, inductive_resolve);
	if (!type) {
		return type.error();
	}

	std::string id = varname_ ? *varname_ : std::string("_");
	auto new_locals_map = locals_map.push(id);
	auto new_locals_types = locals_types.push({id, value.value().check(make_type_context(locals_types, globals_resolve))});

	auto body = body_->resolve(new_locals_map, new_locals_types, globals_resolve, inductive_resolve);
	if (!body) {
		return body.error();
	}

	return builder::let(varname_, std::move(value.value()), std::move(type.value()), std::move(body.value()));
}

constr_ast_node_product::~constr_ast_node_product() {
}

parse_result<constr_t, parse_error>
constr_ast_node_product::resolve(
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) const {
	auto new_locals_map = locals_map;
	auto new_locals_types = locals_types;

	std::vector<formal_arg_t> args;

	for (const auto& arg : args_) {
		auto type = arg.type->resolve(new_locals_map, new_locals_types, globals_resolve, inductive_resolve);
		if (!type) {
			return type.error();
		}
		new_locals_types = locals_types.push({arg.id, type.value()});
		new_locals_map = new_locals_map.push(arg.id);
		args.push_back(formal_arg_t { arg.id, type.value() });
	}

	auto restype = restype_->resolve(new_locals_map, new_locals_types, globals_resolve, inductive_resolve);
	if (!restype) {
		return restype.error();
	}

	return builder::product(std::move(args), restype.move_value());
}

constr_ast_node_lambda::~constr_ast_node_lambda() {
}

parse_result<constr_t, parse_error>
constr_ast_node_lambda::resolve(
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) const {
	auto new_locals_map = locals_map;
	auto new_locals_types = locals_types;

	std::vector<formal_arg_t> args;

	for (const auto& arg : args_) {
		auto type = arg.type->resolve(new_locals_map, new_locals_types, globals_resolve, inductive_resolve);
		if (!type) {
			return type.error();
		}
		new_locals_types = locals_types.push({arg.id, type.value()});
		new_locals_map = new_locals_map.push(arg.id);
		args.push_back(formal_arg_t { arg.id, type.value() });
	}

	auto body = body_->resolve(new_locals_map, new_locals_types, globals_resolve, inductive_resolve);
	if (!body) {
		return body.error();
	}

	return builder::lambda(std::move(args), body.move_value());
}

constr_ast_node_match::~constr_ast_node_match() {
}

parse_result<constr_t, parse_error>
constr_ast_node_match::resolve(
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) const {
	auto arg = arg_->resolve(locals_map, locals_types, globals_resolve, inductive_resolve);
	if (!arg) {
		return arg.error();
	}

	auto arg_type = arg.value().check(make_type_context(locals_types, globals_resolve));

	auto ind = inductive_resolve(arg_type);
	if (!ind) {
		return parse_error { "pattern matching requires an inductive type", location() };
	}

	std::string id = as_id_ ? *as_id_ : "";
	auto new_locals_map = locals_map.push(id);
	auto new_locals_types = locals_types.push({id, arg.value().check(make_type_context(locals_types, globals_resolve))});

	auto restype = restype_->resolve(new_locals_map, new_locals_types, globals_resolve, inductive_resolve);
	if (!restype) {
		return restype.error();
	}
	auto casetype = builder::lambda({{id, arg_type}}, restype.move_value());

	std::vector<match_branch_t> branches;
	for (const auto& branch : branches_) {
		auto cons = lookup_constructor(*ind, branch.constructor);
		if (!cons) {
			return parse_error { "unknown constructor: '" + branch.constructor + "'", location() };
		}

		auto new_locals_map = locals_map;
		auto new_locals_types = locals_types;
		std::vector<formal_arg_t> formal_args;
		auto arg_types = get_constructor_arguments(*cons);

		std::size_t n = 0;
		for (const auto& arg : branch.args) {
			new_locals_map = new_locals_map.push(arg);
			new_locals_types = new_locals_types.push({arg, arg_types[n]});
			formal_args.push_back({arg, arg_types[n]});
			++n;
		}

		auto branch_body = branch.expr->resolve(new_locals_map, new_locals_types, globals_resolve, inductive_resolve);
		if (!branch_body) {
			return branch_body.error();
		}

		// If this is a parameterized inductive, then the parametrisation of
		// the constructor should be omitted during.
		std::size_t arg_count = formal_args.size();
		auto branch_expr = formal_args.size() ?
			builder::lambda(std::move(formal_args), branch_body.move_value()) :
			branch_body.move_value();

		branches.push_back(match_branch_t { branch.constructor, arg_count, std::move(branch_expr) });
	}

	return builder::match(std::move(casetype), arg.move_value(), std::move(branches));
}

token_parser::token_parser(std::istream& is) : is_(is) {
	next_char();
	index_ = 0;
	continue_parsing();
}

const std::optional<token_t>&
token_parser::peek() {
	return current_;
}

std::optional<token_t>
token_parser::get() {
	std::optional<token_t> result = std::move(current_);
	continue_parsing();
	return result;
}

void
token_parser::continue_parsing()
{
	while (next_char_ && is_whitespace(*next_char_)) {
		next_char();
	}

	std::size_t loc = index_;

	if (!next_char_) {
		current_ = std::nullopt;
		return;
	}

	if (is_identifier_start(*next_char_)) {
		std::string id;
		for (;;) {
			id += *next_char_;
			next_char();
			if (!next_char_) {
				break;
			}
			if (*next_char_ == '.') {
				next_next_char();
				if (next_next_char_ && is_identifier_start(*next_next_char_)) {
					continue;
				} else {
					break;
				}
			} else if (!is_identifier_cont(*next_char_)) {
				break;
			}
		}

		for (const auto& entry : keyword_mapping) {
			if (entry.str == id) {
				current_ = token_keyword { entry.code, loc };
				return;
			}
		}

		current_ = token_identifier { std::move(id), loc };

		return;
	}

	std::string sym;
	do {
		sym += *next_char_;
		next_char();

		if (next_char_) {
			std::string tmp = sym + *next_char_;
			if (!lookup_symbol(tmp)) {
				break;
			}
		}
	} while (next_char_);

	auto sym_code = lookup_symbol(sym);
	if (sym_code) {
		current_ = token_symbol { *sym_code, loc };
	} else {
		current_ = token_invalid { sym, loc };
	}
}

void
token_parser::next_char() {
	if (next_next_char_) {
		next_char_ = std::move(next_next_char_);
		next_next_char_ = std::nullopt;
		++index_;
	} else {
		int c = is_.get();
		if (c != EOF) {
			next_char_ = static_cast<char>(c);
			++index_;
		} else {
			next_char_ = std::nullopt;
		}
	}
}

void
token_parser::next_next_char() {
	int c = is_.get();
	if (c != EOF) {
		next_next_char_ = static_cast<char>(c);
	} else {
		next_next_char_ = std::nullopt;
	}
}


parse_result<std::string, parse_error>
parse_id(
	token_parser& tokenizer
) {
	auto tok = tokenizer.get();

	if (!tok) {
		return parse_error { "unexpected end of stream", tokenizer.location() };
	}

	return std::visit(
		[](const auto& arg) -> parse_result<std::string, parse_error>
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same<T, token_identifier>()) {
				return arg.identifier;
			} else {
				return parse_error { "expected identifier", arg.location };
			}
		},
		*tok);
}

parse_result<keyword_t, parse_error>
parse_keyword(
	token_parser& tokenizer
) {
	auto tok = tokenizer.get();

	if (!tok) {
		return parse_error { "unexpected end of stream", tokenizer.location() };
	}

	return std::visit(
		[](const auto& arg) -> parse_result<keyword_t, parse_error>
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same<T, token_keyword>()) {
				return arg.keyword;
			} else {
				return parse_error { "expected keyword", arg.location };
			}
		},
		*tok);
}


parse_result<keyword_t, parse_error>
parse_expect_keyword(
	token_parser& tokenizer,
	keyword_t keyword
) {
	auto tok = tokenizer.get();

	if (!tok) {
		return parse_error { "unexpected end of stream", tokenizer.location() };
	}

	return std::visit(
		[keyword](const auto& arg) -> parse_result<keyword_t, parse_error>
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same<T, token_keyword>()) {
				if (keyword == arg.keyword) {
					return keyword;
				} else {
					return parse_error { " expected '" + keyword_name(keyword) + "'", arg.location };
				}
			} else {
				return parse_error { "expected id", arg.location };
			}
		},
		*tok);
}

parse_result<symbol_t, parse_error>
parse_expect_symbol(
	token_parser& tokenizer,
	symbol_t symbol
) {
	auto tok = tokenizer.get();

	if (!tok) {
		return parse_error { "unexpected end of stream", tokenizer.location() };
	}

	return std::visit(
		[symbol](const auto& arg) -> parse_result<symbol_t, parse_error>
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same<T, token_symbol>()) {
				if (symbol == arg.symbol) {
					return symbol;
				} else {
					return parse_error { " expected '" + symbol_name(symbol) + "'", arg.location };
				}
			} else {
				return parse_error { "expected id", arg.location };
			}
		},
		*tok);
}

parse_result<bool, parse_error>
parse_branch_or_end_of_match(token_parser& tokenizer) {
	auto tok = tokenizer.get();

	if (!tok) {
		return parse_error { "unexpected end of stream", tokenizer.location() };
	}

	return std::visit(
		[](const auto& arg) -> parse_result<bool, parse_error>
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same<T, token_keyword>()) {
				if (arg.keyword == keyword_end) {
					return true;
				}
			} else if constexpr (std::is_same<T, token_symbol>()) {
				if (arg.symbol == symbol_pipe) {
					return false;
				}
			}
			return parse_error { "expected branch or 'end'", arg.location };
		},
		*tok);
}

parse_result<std::shared_ptr<const constr_ast_node>, parse_error>
parse_constr_ast(
	token_parser& tokenizer
);

parse_result<std::vector<std::string>, parse_error>
parse_constr_ast_idlist(
	token_parser& tokenizer
) {
	std::vector<std::string> ids;
	for (;;) {
		const auto& tok = tokenizer.peek();
		if (!tok) {
			break;
		}
		auto have_id = std::visit(
			[&ids, &tokenizer](const auto& arg) -> parse_result<bool, parse_error>
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same<T, token_identifier>()) {
					ids.push_back(arg.identifier);
					tokenizer.get();
					return true;
				} else {
					return false;
				}
			},
			*tok);
		if (!have_id) {
			return have_id.error();
		}
		if (!have_id.value()) {
			break;
		}
	}

	return std::move(ids);
}

parse_result<std::vector<constr_ast_formarg>, parse_error>
parse_constr_ast_formarg(
	token_parser& tokenizer
) {
	auto open = parse_expect_symbol(tokenizer, symbol_open_paren);
	if (!open) {
		return open.error();
	}
	auto ids = parse_constr_ast_idlist(tokenizer);
	if (!ids) {
		return ids.error();
	}
	auto colon = parse_expect_symbol(tokenizer, symbol_colon);
	if (!colon) {
		return colon.error();
	}
	auto type = parse_constr_ast(tokenizer);
	if (!type) {
		return type.error();
	}
	auto close = parse_expect_symbol(tokenizer, symbol_close_paren);
	if (!close) {
		return close.error();
	}

	std::vector<constr_ast_formarg> args;
	for (const auto& id : ids.value()) {
		args.push_back(constr_ast_formarg { id, type.value() });
	}

	return std::move(args);
}

parse_result<std::vector<constr_ast_formarg>, parse_error>
parse_constr_ast_formargs(
	token_parser& tokenizer
) {
	std::vector<constr_ast_formarg> args;

	for (;;) {
		auto tok = tokenizer.peek();
		if (!tok) {
			break;
		}
		bool is_paren = std::visit(
			[](const auto& arg)
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same<T, token_symbol>()) {
					return arg.symbol == symbol_open_paren;
				}
				return false;
			},
			*tok);
		if (!is_paren) {
			break;
		}

		auto tmp = parse_constr_ast_formarg(tokenizer);
		if (!tmp) {
			return tmp.error();
		}
		args.insert(args.end(), tmp.value().begin(), tmp.value().end());
	}

	return std::move(args);
}

parse_result<std::shared_ptr<const constr_ast_node>, parse_error>
parse_constr_ast_inner(
	token_parser& tokenizer
) {
	auto loc = tokenizer.location();
	auto tok = tokenizer.get();

	if (!tok) {
		return parse_error { "unexpected end of stream", loc };
	}

	return std::visit(
		[loc, &tokenizer](const auto& arg) -> parse_result<std::shared_ptr<const constr_ast_node>, parse_error>
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same<T, token_identifier>()) {
				return constr_ast_node_id::create(arg.location, arg.identifier);
			} else if constexpr (std::is_same<T, token_keyword>()) {
				switch (arg.keyword) {
					case keyword_let: {
						auto id = parse_id(tokenizer);
						if (!id) {
							return id.error();
						}
						auto colon = parse_expect_symbol(tokenizer, symbol_colon);
						if (!colon) {
							return colon.error();
						}
						auto type = parse_constr_ast(tokenizer);
						if (!type) {
							return type.error();
						}
						auto assign = parse_expect_symbol(tokenizer, symbol_assign);
						if (!assign) {
							return assign.error();
						}
						auto expr = parse_constr_ast(tokenizer);
						if (!expr) {
							return expr.error();
						}
						auto kw_in = parse_expect_keyword(tokenizer, keyword_in);
						if (!kw_in) {
							return kw_in.error();
						}
						auto body = parse_constr_ast(tokenizer);
						if (!body) {
							return body.error();
						}

						return constr_ast_node_let::create(loc, id.move_value(), expr.move_value(), type.move_value(), body.move_value());
					}
					case keyword_match: {
						auto arg = parse_constr_ast(tokenizer);
						if (!arg) {
							return arg.error();
						}

						auto as = parse_expect_keyword(tokenizer, keyword_as);
						if (!as) {
							return as.error();
						}

						auto as_id = parse_id(tokenizer);
						if (!as_id) {
							return as_id.error();
						}

						auto ret = parse_expect_keyword(tokenizer, keyword_return);
						if (!ret) {
							return ret.error();
						}

						auto restype = parse_constr_ast(tokenizer);
						if (!restype) {
							return restype.error();
						}

						auto with = parse_expect_keyword(tokenizer, keyword_with);
						if (!with) {
							return with.error();
						}

						std::vector<constr_ast_node_match::branch> branches;

						for (;;) {
							auto terminate = parse_branch_or_end_of_match(tokenizer);
							if (!terminate) {
								return terminate.error();
							}

							if (terminate.value()) {
								break;
							}

							auto constr_name = parse_id(tokenizer);
							if (!constr_name) {
								return constr_name.error();
							}

							std::vector<std::string> args;
							for (;;) {
								auto tmp_loc = tokenizer.location();
								auto tok = tokenizer.get();
								if (!tok) {
									return parse_error { "unexpected end of stream", tmp_loc };
								}

								auto end_of_args = std::visit(
									[&args](const auto& arg) -> parse_result<bool, parse_error>
									{
										using T = std::decay_t<decltype(arg)>;
										if constexpr (std::is_same<T, token_identifier>()) {
											args.push_back(arg.identifier);
											return false;
										} else if constexpr (std::is_same<T, token_symbol>()) {
											if (arg.symbol == symbol_mapsto) {
												return true;
											}
										}
										return parse_error { "Expected identifier or '=>'", arg.location };
									},
									*tok);
								if (!end_of_args) {
									return end_of_args.error();
								}
								if (end_of_args.value()) {
									break;
								}
							}

							auto expr = parse_constr_ast(tokenizer);
							if (!expr) {
								return expr.error();
							}

							branches.push_back(constr_ast_node_match::branch { constr_name.move_value(), std::move(args), expr.move_value() });
						}

						return constr_ast_node_match::create(loc, restype.move_value(), arg.move_value(), as_id.move_value(), std::move(branches));
					}
					case keyword_forall: {
						auto args = parse_constr_ast_formargs(tokenizer);
						if (!args) {
							return args.error();
						}

						auto comma = parse_expect_symbol(tokenizer, symbol_comma);
						if (!comma) {
							return comma.error();
						}

						auto restype = parse_constr_ast(tokenizer);
						if (!restype) {
							return restype.error();
						}

						return constr_ast_node_product::create(loc, args.move_value(), restype.move_value());
					}
					case keyword_fun: {
						auto args = parse_constr_ast_formargs(tokenizer);
						if (!args) {
							return args.error();
						}

						auto mapsto = parse_expect_symbol(tokenizer, symbol_mapsto);
						if (!mapsto) {
							return mapsto.error();
						}

						auto body = parse_constr_ast(tokenizer);
						if (!body) {
							return body.error();
						}

						return constr_ast_node_lambda::create(loc, args.move_value(), body.move_value());
					}
					default: {
						return parse_error { "unexpected keyword " + keyword_name(arg.keyword), arg.location };
					}
				}
			} else if constexpr (std::is_same<T, token_symbol>()) {
				switch (arg.symbol) {
					case symbol_open_paren: {
						auto res = parse_constr_ast(tokenizer);
						if (!res) {
							return res.error();
						}
						auto close = parse_expect_symbol(tokenizer, symbol_close_paren);
						if (!close) {
							return close.error();
						}
						return res;
					}
					default: {
						return parse_error { "unexpected symbol " + symbol_name(arg.symbol), arg.location };
					}
				}
			} else if constexpr (std::is_same<T, token_invalid>()) {
				return parse_error { "invalid token " + arg.content, arg.location };
			} else {
				return parse_error { "unexpected kind of token ", arg.location };
			}
		},
		*tok);
}

parse_result<std::shared_ptr<const constr_ast_node>, parse_error>
parse_constr_ast_apply(
	token_parser& tokenizer
) {
	auto loc = tokenizer.location();
	auto res = parse_constr_ast_inner(tokenizer);
	if (!res) {
		return res.error();
	}

	std::shared_ptr<const constr_ast_node> fn = res.move_value();
	std::vector<std::shared_ptr<const constr_ast_node>> args;

	for (;;) {
		auto tok = tokenizer.peek();
		if (!tok) {
			break;
		}

		bool terminates_call = std::visit(
			[](const auto& arg) -> bool
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same<T, token_identifier>()) {
					return false;
				} else if constexpr (std::is_same<T, token_keyword>()) {
					switch (arg.keyword) {
						case keyword_end:
						case keyword_in:
						case keyword_with: {
							return true;
						}
						case keyword_match:
						case keyword_forall:
						case keyword_let: {
							return false;
						}
						case keyword_definition:
						case keyword_fixpoint:
						case keyword_inductive:
						default: {
							// Note that these will actually lead to parse errors.
							return true;
						}
					}
				} else if constexpr (std::is_same<T, token_symbol>()) {
					switch (arg.symbol) {
						case symbol_colon:
						case symbol_equals:
						case symbol_assign:
						case symbol_close_paren:
						case symbol_arrow:
						case symbol_mapsto: {
							return true;
						}
						case symbol_open_paren: {
							return false;
						}
						case symbol_dot:
						default: {
							// Note that these will actually lead to parse errors.
							return true;
						}
					}
				} else {
					return true;
				}
			},
			*tok
		);
		if (terminates_call) {
			break;
		}

		auto res = parse_constr_ast_inner(tokenizer);
		if (!res) {
			return res.error();
		}

		args.push_back(res.move_value());
	}

	if (args.empty()) {
		return std::move(fn);
	} else {
		return constr_ast_node_apply::create(loc, std::move(fn), std::move(args));
	}
}

parse_result<std::shared_ptr<const constr_ast_node>, parse_error>
parse_constr_ast(
	token_parser& tokenizer
) {
	return parse_constr_ast_apply(tokenizer);
}

parse_result<constr_t, parse_error>
parse_constr(
	token_parser& tokenizer,
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) {
	auto node = parse_constr_ast(tokenizer);
	if (!node) {
		return node.error();
	}

	return node.value()->resolve(locals_map, locals_types, globals_resolve, inductive_resolve);
}

parse_result<constr_t, parse_error>
parse_constr(
	const std::string& s,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) {
	std::stringstream ss(s);
	token_parser tokenizer(ss);
	return parse_constr(tokenizer, {}, lazy_stack<type_context_t::local_entry>{}, globals_resolve, inductive_resolve);
}

struct sfb_ast_consdef {
	std::string id;
	std::shared_ptr<const constr_ast_node> type;
};

parse_result<sfb_ast_consdef, parse_error>
parse_sfb_consdef(
	token_parser& tokenizer
) {
	auto id = parse_id(tokenizer);
	if (!id) {
		return id.error();
	}

	auto colon = parse_expect_symbol(tokenizer, symbol_colon);
	if (!colon) {
		return colon.error();
	}

	auto expr = parse_constr_ast(tokenizer);
	if (!expr) {
		return expr.error();
	}

	return sfb_ast_consdef { id.move_value(), expr.move_value() };
}

struct sfb_ast_one_inductive {
	std::string id;
	std::vector<constr_ast_formarg> args;
	std::shared_ptr<const constr_ast_node> type;
	std::vector<sfb_ast_consdef> constructors;
};

parse_result<sfb_ast_one_inductive, parse_error>
parse_sfb_one_inductive(
	token_parser& tokenizer
) {
	auto id = parse_id(tokenizer);
	if (!id) {
		return id.error();
	}

	auto formargs = parse_constr_ast_formargs(tokenizer);
	if (!formargs) {
		return formargs.error();
	}

	auto colon = parse_expect_symbol(tokenizer, symbol_colon);
	if (!colon) {
		return colon.error();
	}

	auto type = parse_constr_ast(tokenizer);
	if (!type) {
		return type.error();
	}

	auto assign = parse_expect_symbol(tokenizer, symbol_assign);
	if (!assign) {
		return assign.error();
	}

	std::vector<sfb_ast_consdef> constructors;

	for (;;) {
		const auto& tok = tokenizer.peek();
		if (!tok || !token_is_symbol(*tok, symbol_pipe)) {
			break;
		}
		tokenizer.get();

		auto cons = parse_sfb_consdef(tokenizer);
		if (!cons) {
			return cons.error();
		}

		constructors.push_back(cons.move_value());
	}

	return sfb_ast_one_inductive { id.move_value(), formargs.move_value(), type.move_value(), std::move(constructors) };
}

namespace {

std::string
make_mod_id(const std::string& mod_context, const std::string& id) {
	return mod_context.empty() ? id : mod_context + "." + id;
}

parse_result<sfb_t, parse_error>
parse_sfb_definition(
	token_parser& tokenizer,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve,
	parse_symtab_t& symtab,
	const std::string mod_context
) {
	auto combined_globals_resolve = [&globals_resolve, &symtab](const std::string& id) -> std::optional<constr_t> {
		auto i = symtab.id_to_type.find(id);
		return i != symtab.id_to_type.end() ? i->second : globals_resolve(id);
	};
	auto combined_inductive_resolve = [&inductive_resolve, &symtab](const constr_t& constr) -> std::optional<one_inductive_t> {
		constr_t inner = constr;
		while (auto a = inner.as_apply()) {
			inner = a->fn();
		}

		if (auto g = inner.as_global()) {
			auto i = symtab.id_to_inductive.find(g->name());
			if (i != symtab.id_to_inductive.end()) {
				return i->second;
			}
		}
		return inductive_resolve(constr);
	};

	auto id = parse_id(tokenizer);
	if (!id) {
		return id.error();
	}

	auto colon = parse_expect_symbol(tokenizer, symbol_colon);
	if (!colon) {
		return colon.error();
	}

	auto type = parse_constr(tokenizer, {}, {}, combined_globals_resolve, combined_inductive_resolve);
	if (!type) {
		return type.error();
	}

	auto assign = parse_expect_symbol(tokenizer, symbol_assign);
	if (!assign) {
		return assign.error();
	}

	auto expr = parse_constr(tokenizer, {}, {}, combined_globals_resolve, combined_inductive_resolve);
	if (!expr) {
		return expr.error();
	}

	symtab.id_to_type[make_mod_id(mod_context, id.value())] = type.value();

	return builder::definition(id.move_value(), type.move_value(), expr.move_value());
}

parse_result<sfb_t, parse_error>
parse_sfb_inductive(
	token_parser& tokenizer,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve,
	parse_symtab_t& symtab,
	const std::string mod_context
) {
	auto combined_globals_resolve = [&globals_resolve, &symtab](const std::string& id) -> std::optional<constr_t> {
		auto i = symtab.id_to_type.find(id);
		return i != symtab.id_to_type.end() ? i->second : globals_resolve(id);
	};
	auto combined_inductive_resolve = [&inductive_resolve, &symtab](const constr_t& constr) -> std::optional<one_inductive_t> {
		constr_t inner = constr;
		while (auto a = inner.as_apply()) {
			inner = a->fn();
		}

		if (auto g = inner.as_global()) {
			auto i = symtab.id_to_inductive.find(g->name());
			if (i != symtab.id_to_inductive.end()) {
				return i->second;
			}
		}
		return inductive_resolve(constr);
	};

	std::vector<sfb_ast_one_inductive> ast_oinds;

	for (;;) {
		auto oind = parse_sfb_one_inductive(tokenizer);
		if (!oind) {
			return oind.error();
		}
		ast_oinds.push_back(oind.move_value());
		auto tok = tokenizer.peek();
		if (!tok || !token_is_keyword(*tok, keyword_with)) {
			break;
		}
		tokenizer.get();
	}

	lazy_stackmap<std::string> locals_map;
	lazy_stack<type_context_t::local_entry> locals_types;

	std::vector<one_inductive_t> oinds;

	// Push the ids of the inductives as globals for resolving
	// their type.
	std::unordered_map<std::string, constr_t> inductive_globals;
	for (const auto& oind : ast_oinds) {
		std::vector<formal_arg_t> formargs;
		for (const auto& ast_formarg : oind.args) {
			auto formarg = ast_formarg.resolve({}, {}, combined_globals_resolve, combined_inductive_resolve);
			if (!formarg) {
				return formarg.error();
			}
			formargs.push_back(formarg.move_value());
		}

		auto type = oind.type->resolve({}, {}, combined_globals_resolve, combined_inductive_resolve);
		if (!type) {
			return type.error();
		}

		auto restype = formargs.empty() ? type.move_value() : normalize(builder::product(formargs, type.move_value()));
		inductive_globals[oind.id] = restype;

		oinds.push_back(one_inductive_t{oind.id, restype, {}});
	}

	auto globals_resolve_inductive = [
		&inductive_globals,
		&combined_globals_resolve
	](const std::string& id) -> std::optional<constr_t> {
		auto i = inductive_globals.find(id);
		return i == inductive_globals.end() ? combined_globals_resolve(id) : i->second;
	};

	// Now, resolve all constructors.
	for (std::size_t n = 0; n < ast_oinds.size(); ++n) {
		// The arguments of the inductive type itself need to be added
		// as additional arguments to each constructor.
		auto ind_locals_types = locals_types;
		auto ind_locals_map = locals_map;
		auto& ast_oind = ast_oinds[n];
		auto& oind = oinds[n];
		std::vector<formal_arg_t> formargs;
		for (const auto& ast_formarg : ast_oind.args) {
			auto formarg = ast_formarg.resolve({}, {}, globals_resolve_inductive, combined_inductive_resolve);
			if (!formarg) {
				return formarg.error();
			}
			ind_locals_types = ind_locals_types.push({formarg.value().name.value_or("_"), formarg.value().type});
			ind_locals_map = ind_locals_map.push(formarg.value().name.value_or("_"));
			formargs.push_back(formarg.move_value());
		}
		for (const auto& cons : ast_oind.constructors) {
			auto type = cons.type->resolve(ind_locals_map, ind_locals_types, globals_resolve_inductive, combined_inductive_resolve);
			if (!type) {
				return type.error();
			}
			auto constype = formargs.empty() ? normalize(type.move_value()) : normalize(builder::product(formargs, type.move_value()));
			oind.constructors.push_back(constructor_t {cons.id, std::move(constype)} );
		}
	}

	for (const auto& oind : oinds) {
		symtab.id_to_type[make_mod_id(mod_context, oind.id)] = oind.type;
		symtab.id_to_inductive.emplace(make_mod_id(mod_context, oind.id), oind);

		for (const auto& cons : oind.constructors) {
			symtab.id_to_type[make_mod_id(mod_context, cons.id)] = cons.type;
		}
	}

	return builder::inductive(std::move(oinds));
}

parse_result<sfb_t, parse_error>
parse_sfb_fixpoint(
	token_parser& tokenizer,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve,
	parse_symtab_t& symtab,
	const std::string mod_context
) {
	auto combined_globals_resolve = [&globals_resolve, &symtab](const std::string& id) -> std::optional<constr_t> {
		auto i = symtab.id_to_type.find(id);
		return i != symtab.id_to_type.end() ? i->second : globals_resolve(id);
	};
	auto combined_inductive_resolve = [&inductive_resolve, &symtab](const constr_t& constr) -> std::optional<one_inductive_t> {
		constr_t inner = constr;
		while (auto a = inner.as_apply()) {
			inner = a->fn();
		}

		if (auto g = inner.as_global()) {
			auto i = symtab.id_to_inductive.find(g->name());
			if (i != symtab.id_to_inductive.end()) {
				return i->second;
			}
		}
		return inductive_resolve(constr);
	};

	struct sfb_ast_fix_function_t {
		std::string name;
		std::vector<constr_ast_formarg> args;
		std::shared_ptr<const constr_ast_node> restype;
		std::shared_ptr<const constr_ast_node> body;
	};

	std::vector<sfb_ast_fix_function_t> ast_group;

	for (;;) {
		auto id = parse_id(tokenizer);
		if (!id) {
			return id.error();
		}

		auto args = parse_constr_ast_formargs(tokenizer);
		if (!args) {
			return args.error();
		}

		auto colon = parse_expect_symbol(tokenizer, symbol_colon);
		if (!colon) {
			return colon.error();
		}

		auto restype = parse_constr_ast(tokenizer);
		if (!restype) {
			return restype.error();
		}

		auto assign = parse_expect_symbol(tokenizer, symbol_assign);
		if (!assign) {
			return assign.error();
		}
		auto body = parse_constr_ast(tokenizer);
		if (!body) {
			return body.error();
		}

		ast_group.push_back(
			sfb_ast_fix_function_t {id.move_value(), args.move_value(), restype.move_value(), body.move_value()}
		);

		auto tok = tokenizer.peek();
		if (!tok || !token_is_keyword(*tok, keyword_with)) {
			break;
		}
		tokenizer.get();
	}

	struct fn_sig_t {
		std::string name;
		std::vector<formal_arg_t> args;
		constr_t restype;
		std::shared_ptr<const constr_ast_node> body;
	};
	std::vector<fn_sig_t> sigs;

	// First step: resolve the function types.
	for (const auto& fn : ast_group) {
		std::vector<formal_arg_t> args;
		lazy_stackmap<std::string> locals_map;
		lazy_stack<type_context_t::local_entry> locals_types;

		for (const auto& arg : fn.args) {
			auto type = arg.type->resolve(locals_map, locals_types, combined_globals_resolve, combined_inductive_resolve);
			if (!type) {
				return type.error();
			}
			locals_types = locals_types.push({arg.id, type.value()});
			locals_map = locals_map.push(arg.id);
			args.push_back(formal_arg_t { arg.id, type.value() });
		}

		auto restype = fn.restype->resolve(locals_map, locals_types, combined_globals_resolve, combined_inductive_resolve);
		if (!restype) {
			return restype.error();
		}

		sigs.push_back(
			fn_sig_t {
				fn.name,
				std::move(args),
				restype.move_value(),
				std::move(fn.body)
			});
	}

	lazy_stackmap<std::string> locals_map;
	lazy_stack<type_context_t::local_entry> locals_types;
	// Push function names and signatures as local context variables.
	for (const auto& sig : sigs) {
		auto type = builder::product(sig.args, sig.restype);
		locals_types = locals_types.push({sig.name, type});
		locals_map = locals_map.push(sig.name);
	}

	fix_group_t group;
	// Now resolve function bodies and build fix group.
	for (const auto& sig : sigs) {
		auto inner_locals_types = locals_types;
		auto inner_locals_map = locals_map;
		for (const auto& arg : sig.args) {
			inner_locals_types = inner_locals_types.push({arg.name ? *arg.name : "_", arg.type});
			inner_locals_map = inner_locals_map.push(arg.name ? *arg.name : "_");
		}

		auto body = sig.body->resolve(inner_locals_map, inner_locals_types, combined_globals_resolve, combined_inductive_resolve);
		if (!body) {
			return body.error();
		}

		group.functions.push_back(
			fix_function_t {
				std::move(sig.name),
				std::move(sig.args),
				std::move(sig.restype),
				body.move_value()
			});
	}

	for (const auto& fn : group.functions) {
		symtab.id_to_type[make_mod_id(mod_context, fn.name)] = builder::product(fn.args, fn.restype);
	}

	return builder::fixpoint(std::move(group));
}

parse_result<sfb_t, parse_error>
parse_sfb_module(
	token_parser& tokenizer,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve,
	parse_symtab_t& symtab,
	const std::string mod_context
) {
	// Currently, only non-algebraic unparameterized modules are supported.
	auto id = parse_id(tokenizer);
	if (!id) {
		return id.error();
	}

	auto dot = parse_expect_symbol(tokenizer, symbol_dot);
	if (!dot) {
		return dot.error();
	}

	std::string sub_mod_context =
		mod_context.empty() ? id.value() : mod_context + "." + id.value();

	std::vector<sfb_t> sfbs;
	for (;;) {
		auto tok = tokenizer.peek();
		if (!tok || token_is_keyword(*tok, keyword_End)) {
			break;
		}

		auto sfb = parse_sfb(
			tokenizer,
			globals_resolve,
			inductive_resolve,
			symtab,
			sub_mod_context
		);
		if (!sfb) {
			return sfb.error();
		}

		sfbs.push_back(sfb.move_value());

		auto dot = parse_expect_symbol(tokenizer, symbol_dot);
		if (!dot) {
			return dot.error();
		}
	}

	auto end = parse_expect_keyword(tokenizer, keyword_End);
	if (!end) {
		return end.error();
	}

	auto end_id = parse_id(tokenizer);
	if (!end_id) {
		return end_id.error();
	}

	return builder::module_def(
		id.move_value(),
		module_body(
			/* parameters */ {},
			std::make_shared<module_body_struct_repr>(
				/* type */ std::nullopt,
				/* sfbs */ std::move(sfbs)
			)
		)
	);
}

}  // namespace

parse_result<sfb_t, parse_error>
parse_sfb(
	token_parser& tokenizer,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve,
	parse_symtab_t& symtab,
	const std::string mod_context
) {
	auto kw = parse_keyword(tokenizer);
	if (!kw) {
		return kw.error();
	}

	switch (kw.value()) {
		case keyword_definition: {
			return parse_sfb_definition(tokenizer, globals_resolve, inductive_resolve, symtab, mod_context);
		}
		case keyword_inductive: {
			return parse_sfb_inductive(tokenizer, globals_resolve, inductive_resolve, symtab, mod_context);
		}
		case keyword_fixpoint: {
			return parse_sfb_fixpoint(tokenizer, globals_resolve, inductive_resolve, symtab, mod_context);
		}
		case keyword_module: {
			return parse_sfb_module(tokenizer, globals_resolve, inductive_resolve, symtab, mod_context);
		}
		default: {
			return parse_error { "Expected 'Definition' or 'Fixpoint' or 'Inductive' or 'Module'", tokenizer.location() };
		}
	}
}

parse_result<sfb_t, parse_error>
parse_sfb(
	const std::string& s,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve,
	parse_symtab_t& symtab,
	const std::string mod_context
) {
	std::stringstream ss(s);
	token_parser tokenizer(ss);
	return parse_sfb(tokenizer, globals_resolve, inductive_resolve, symtab, mod_context);
}

parse_result<sfb_t, parse_error>
parse_sfb(
	const std::string& s,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
) {
	parse_symtab_t symtab;
	return parse_sfb(s, globals_resolve, inductive_resolve, symtab, "");
}

}  // namespace mgl
}  // namespace coqcic
