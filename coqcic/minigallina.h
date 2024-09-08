#ifndef COQCIC_MINIGALLINA_H
#define COQCIC_MINIGALLINA_H

#include "coqcic/constr.h"
#include "coqcic/lazy_stack.h"
#include "coqcic/lazy_stackmap.h"
#include "coqcic/parse_result.h"
#include "coqcic/sfb.h"

#include <istream>
#include <optional>

namespace coqcic {
namespace mgl {

enum keyword_t {
	keyword_definition,
	keyword_fixpoint,
	keyword_inductive,
	keyword_module,
	keyword_End,
	keyword_end,
	keyword_match,
	keyword_with,
	keyword_forall,
	keyword_let,
	keyword_in,
	keyword_as,
	keyword_return,
	keyword_fun,
	keyword_fix,
	keyword_for
};

enum symbol_t {
	symbol_dot,
	symbol_comma,
	symbol_colon,
	symbol_equals,
	symbol_assign,
	symbol_open_paren,
	symbol_close_paren,
	symbol_arrow,
	symbol_mapsto,
	symbol_pipe
};

struct token_keyword {
	keyword_t keyword;
	std::size_t location;
};

struct token_symbol {
	symbol_t symbol;
	std::size_t location;
};

struct token_identifier {
	std::string identifier;
	std::size_t location;
};

struct token_invalid {
	std::string content;
	std::size_t location;
};

using token_t = std::variant<
	token_keyword,
	token_symbol,
	token_identifier,
	token_invalid
>;

class token_parser {
public:
	token_parser(std::istream& is);

	const std::optional<token_t>&
	peek();

	std::optional<token_t>
	get();

	inline std::size_t
	location() const noexcept { return index_; }

private:
	void
	continue_parsing();

	void
	next_char();

	void
	next_next_char();

	std::optional<token_t> current_;
	std::optional<char> next_char_;
	std::optional<char> next_next_char_;
	std::istream& is_;
	std::size_t index_ = 0;
};

struct parse_error {
	std::string description;
	// character index into source
	std::size_t location;
};

struct parse_symtab_t {
	std::unordered_map<std::string, constr_t> id_to_type;
	std::unordered_map<std::string, one_inductive_t> id_to_inductive;
};

class constr_ast_node {
public:
	virtual ~constr_ast_node();

	virtual
	parse_result<constr_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const = 0;

	inline std::size_t location() const noexcept { return location_; }

protected:
	explicit constr_ast_node(std::size_t location) : location_(location) {}

private:
	std::size_t location_;
};

struct constr_ast_formarg {
	std::string id;
	std::shared_ptr<const constr_ast_node> type;

	parse_result<formal_arg_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const;
};

class constr_ast_node_id final : public constr_ast_node {
private:
	struct private_tag {};

public:
	~constr_ast_node_id() override;

	parse_result<constr_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const override;

	inline const std::string& id() const noexcept { return id_; }

	inline static
	std::shared_ptr<const constr_ast_node>
	create(
		std::size_t location,
		std::string id
	) {
		return std::make_shared<constr_ast_node_id>(location, std::move(id), private_tag{});
	}

	inline
	constr_ast_node_id(
		std::size_t location,
		std::string id,
		private_tag
	) : constr_ast_node(location), id_(std::move(id)) {
	}

private:
	std::string id_;
};

class constr_ast_node_apply final : public constr_ast_node {
private:
	struct private_tag {};

public:
	~constr_ast_node_apply() override;

	parse_result<constr_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const override;

	inline const std::shared_ptr<const constr_ast_node>& fn() const noexcept { return fn_; }

	inline const std::vector<std::shared_ptr<const constr_ast_node>>& args() const noexcept { return args_; }

	inline static
	std::shared_ptr<const constr_ast_node>
	create(
		std::size_t location,
		std::shared_ptr<const constr_ast_node> fn,
		std::vector<std::shared_ptr<const constr_ast_node>> args
	) {
		return std::make_shared<constr_ast_node_apply>(location, std::move(fn), std::move(args), private_tag{});
	}

	inline
	constr_ast_node_apply(
		std::size_t location,
		std::shared_ptr<const constr_ast_node> fn,
		std::vector<std::shared_ptr<const constr_ast_node>> args,
		private_tag
	) : constr_ast_node(location), fn_(std::move(fn)), args_(std::move(args)) {
	}

private:
	std::shared_ptr<const constr_ast_node> fn_;
	std::vector<std::shared_ptr<const constr_ast_node>> args_;
};

class constr_ast_node_let final : public constr_ast_node {
private:
	struct private_tag {};

public:
	~constr_ast_node_let() override;

	parse_result<constr_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const override;

	inline const std::optional<std::string>& varname() const noexcept { return varname_; }
	inline const std::shared_ptr<const constr_ast_node>& value() const noexcept { return value_; }
	inline const std::shared_ptr<const constr_ast_node>& type() const noexcept { return type_; }
	inline const std::shared_ptr<const constr_ast_node>& body() const noexcept { return body_; }

	inline static
	std::shared_ptr<const constr_ast_node>
	create(
		std::size_t location,
		std::optional<std::string> varname,
		std::shared_ptr<const constr_ast_node> value,
		std::shared_ptr<const constr_ast_node> type,
		std::shared_ptr<const constr_ast_node> body
	) {
		return std::make_shared<constr_ast_node_let>(
			location,
			std::move(varname),
			std::move(value),
			std::move(type),
			std::move(body),
			private_tag{}
		);
	}

	inline
	constr_ast_node_let(
		std::size_t location,
		std::optional<std::string> varname,
		std::shared_ptr<const constr_ast_node> value,
		std::shared_ptr<const constr_ast_node> type,
		std::shared_ptr<const constr_ast_node> body,
		private_tag
	) : constr_ast_node(location),
		varname_(std::move(varname)),
		value_(std::move(value)),
		type_(std::move(type)),
		body_(std::move(body)) {
	}

private:
	std::optional<std::string> varname_;
	std::shared_ptr<const constr_ast_node> value_;
	std::shared_ptr<const constr_ast_node> type_;
	std::shared_ptr<const constr_ast_node> body_;
};

class constr_ast_node_product final : public constr_ast_node {
private:
	struct private_tag {};

public:
	~constr_ast_node_product() override;

	parse_result<constr_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const override;

	inline const std::vector<constr_ast_formarg>& args() const noexcept { return args_; }
	inline const std::shared_ptr<const constr_ast_node>& restype() const noexcept { return restype_; }

	inline static
	std::shared_ptr<const constr_ast_node>
	create(
		std::size_t location,
		std::vector<constr_ast_formarg> args,
		std::shared_ptr<const constr_ast_node> restype
	) {
		return std::make_shared<constr_ast_node_product>(location, std::move(args), std::move(restype), private_tag{});
	}

	inline
	constr_ast_node_product(
		std::size_t location,
		std::vector<constr_ast_formarg> args,
		std::shared_ptr<const constr_ast_node> restype,
		private_tag
	) : constr_ast_node(location), args_(std::move(args)), restype_(std::move(restype)) {
	}

private:
	std::vector<constr_ast_formarg> args_;
	std::shared_ptr<const constr_ast_node> restype_;
};

class constr_ast_node_lambda final : public constr_ast_node {
private:
	struct private_tag {};

public:
	~constr_ast_node_lambda() override;

	parse_result<constr_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const override;

	inline const std::vector<constr_ast_formarg>& args() const noexcept { return args_; }
	inline const std::shared_ptr<const constr_ast_node>& body() const noexcept { return body_; }

	inline static
	std::shared_ptr<const constr_ast_node>
	create(
		std::size_t location,
		std::vector<constr_ast_formarg> args,
		std::shared_ptr<const constr_ast_node> body
	) {
		return std::make_shared<constr_ast_node_lambda>(location, std::move(args), std::move(body), private_tag{});
	}

	inline
	constr_ast_node_lambda(
		std::size_t location,
		std::vector<constr_ast_formarg> args,
		std::shared_ptr<const constr_ast_node> body,
		private_tag
	) : constr_ast_node(location), args_(std::move(args)), body_(std::move(body)) {
	}

private:
	std::vector<constr_ast_formarg> args_;
	std::shared_ptr<const constr_ast_node> body_;
};

class constr_ast_node_fix final : public constr_ast_node {
private:
	struct private_tag {};

public:
	struct fixfn_t {
		std::string id;
		std::shared_ptr<const constr_ast_node> signature;
		std::shared_ptr<const constr_ast_node> body;
	};

	~constr_ast_node_fix() override;

	parse_result<constr_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const override;

	inline static
	std::shared_ptr<const constr_ast_node>
	create(
		std::size_t location,
		std::vector<fixfn_t> fns,
		std::string call
	) {
		return std::make_shared<constr_ast_node_fix>(location, std::move(fns), std::move(call), private_tag{});
	}

	inline
	constr_ast_node_fix(
		std::size_t location,
		std::vector<fixfn_t> fns,
		std::string call,
		private_tag
	) : constr_ast_node(location), fns_(std::move(fns)), call_(std::move(call)) {
	}

private:
	std::size_t location_;
	std::vector<fixfn_t> fns_;
	std::string call_;
};

class constr_ast_node_match final : public constr_ast_node {
private:
	struct private_tag {};

public:
	struct branch {
		std::string constructor;
		std::vector<std::string> args;
		std::shared_ptr<const constr_ast_node> expr;
	};

	~constr_ast_node_match() override;

	parse_result<constr_t, parse_error>
	resolve(
		const lazy_stackmap<std::string>& locals_map,
		const lazy_stack<type_context_t::local_entry>& locals_types,
		const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
		const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
	) const override;

	inline const std::shared_ptr<const constr_ast_node>& restype() const noexcept { return restype_; }
	inline const std::shared_ptr<const constr_ast_node>& arg() const noexcept { return arg_; }
	inline const std::optional<std::string>& as_id() const noexcept { return as_id_; }
	inline const std::vector<branch>& branches() const noexcept { return branches_; }

	inline static
	std::shared_ptr<const constr_ast_node>
	create(
		std::size_t location,
		std::shared_ptr<const constr_ast_node> restype,
		std::shared_ptr<const constr_ast_node> arg,
		std::optional<std::string> as_id,
		std::vector<branch> branches
	) {
		return std::make_shared<constr_ast_node_match>(location, std::move(restype), std::move(arg), std::move(as_id), std::move(branches), private_tag{});
	}

	inline
	constr_ast_node_match(
		std::size_t location,
		std::shared_ptr<const constr_ast_node> restype,
		std::shared_ptr<const constr_ast_node> arg,
		std::optional<std::string> as_id,
		std::vector<branch> branches,
		private_tag
	) : constr_ast_node(location),
		restype_(std::move(restype)),
		arg_(std::move(arg)),
		as_id_(std::move(as_id)),
		branches_(std::move(branches)) {
	}

private:
	std::shared_ptr<const constr_ast_node> restype_;
	std::shared_ptr<const constr_ast_node> arg_;
	std::optional<std::string> as_id_;
	std::vector<branch> branches_;
};

parse_result<std::shared_ptr<const constr_ast_node>, parse_error>
parse_constr_ast(
	token_parser& tokenizer);

parse_result<constr_t, parse_error>
parse_constr(
	token_parser& tokenizer,
	const lazy_stackmap<std::string>& locals_map,
	const lazy_stack<type_context_t::local_entry>& locals_types,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
);

parse_result<constr_t, parse_error>
parse_constr(
	const std::string& s,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
);

parse_result<std::vector<token_t>, parse_error>
parse_constr_tokenstream(
	token_parser& tokenizer
);

parse_result<sfb_t, parse_error>
parse_sfb(
	token_parser& tokenizer,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve,
	parse_symtab_t& symtab,
	const std::string mod_context
);

parse_result<sfb_t, parse_error>
parse_sfb(
	const std::string& s,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve,
	parse_symtab_t& symtab,
	const std::string mod_context
);

parse_result<sfb_t, parse_error>
parse_sfb(
	const std::string& s,
	const std::function<std::optional<constr_t>(const std::string&)>& globals_resolve,
	const std::function<std::optional<one_inductive_t>(const constr_t&)>& inductive_resolve
);


}  // namespace mgl
}  // namespace coqcic

#endif  // COQCIC_MINIGALLINA_H
