#include "coqcic/visitor.h"

namespace coqcic {

transform_visitor::~transform_visitor()
{
}

void
transform_visitor::push_local(
	const std::string* name,
	const constr_t* type,
	const constr_t* value)
{
}

void
transform_visitor::pop_local()
{
}

std::optional<constr_t>
transform_visitor::handle_local(const std::string& name, std::size_t index)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_global(const std::string& name)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_builtin(const std::string& name)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_product(const std::vector<formal_arg_t>& args, const constr_t& restype)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_lambda(const std::vector<formal_arg_t>& args, const constr_t& body)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_let(const std::optional<std::string>& varname, const constr_t& value, const constr_t& body)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_apply(const constr_t& fn, const std::vector<constr_t>& args)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_cast(const constr_t& term, const constr_cast::kind_type kind, const constr_t& typeterm)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_case(const constr_t& argtype, const constr_t& restype, const std::vector<match_branch_t>& branches)
{
	return {};
}

std::optional<constr_t>
transform_visitor::handle_fix(std::size_t index, const std::shared_ptr<const fix_group_t>& group)
{
	return {};
}

std::optional<constr_t>
visit_transform(
	const constr_t& input,
	transform_visitor& visitor)
{
	if (auto local = input.as_local()) {
		return visitor.handle_local(local->name(), local->index());
	} else if (auto global = input.as_global()) {
		return visitor.handle_global(global->name());
	} else if (auto builtin = input.as_builtin()) {
		return visitor.handle_builtin(builtin->name());
	} else if (auto product = input.as_product()) {
		std::vector<formal_arg_t> args;
		bool changed = false;
		for (const auto& arg : product->args()) {
			auto maybe_argtype = visit_transform(arg.type, visitor);
			changed = changed || maybe_argtype;
			args.push_back(maybe_argtype ? formal_arg_t{arg.name, *maybe_argtype} : arg);

			visitor.push_local(arg.name ? &*arg.name : nullptr, &arg.type, nullptr);
		}
		auto maybe_restype = visit_transform(product->restype(), visitor);
		const auto& restype = maybe_restype ? *maybe_restype : product->restype();
		changed = changed || maybe_restype;

		for (const auto& arg : product->args()) {
			(void) arg;
			visitor.pop_local();
		}

		auto result = visitor.handle_product(args, restype);
		if (result) {
			return result;
		} else if (changed) {
			return builder::product(std::move(args), restype);
		} else {
			return {};
		}
	} else if (auto lambda = input.as_lambda()) {
		std::vector<formal_arg_t> args;
		bool changed = false;
		for (const auto& arg : lambda->args()) {
			auto maybe_argtype = visit_transform(arg.type, visitor);
			changed = changed || maybe_argtype;
			args.push_back(maybe_argtype ? formal_arg_t{arg.name, *maybe_argtype} : arg);

			visitor.push_local(arg.name ? &*arg.name : nullptr, &arg.type, nullptr);
		}


		auto maybe_body = visit_transform(lambda->body(), visitor);
		const auto& body = maybe_body ? *maybe_body : lambda->body();
		changed = changed || maybe_body;

		for (const auto& arg : lambda->args()) {
			(void) arg;
			visitor.pop_local();
		}

		auto result = visitor.handle_lambda(args, body);
		if (result) {
			return result;
		} else if (changed) {
			return builder::lambda(std::move(args), body);
		} else {
			return {};
		}
	} else if (auto let = input.as_let()) {
		auto maybe_value = visit_transform(let->value(), visitor);
		const auto& value = maybe_value ? *maybe_value : let->value();

		visitor.push_local(
			let->varname() ? &*let->varname() : nullptr,
			&value,
			nullptr);
		auto maybe_body = visit_transform(let->body(), visitor);
		const auto& body = maybe_body ? *maybe_body : let->body();
		visitor.pop_local();

		auto result = visitor.handle_let(let->varname(), value, body);
		if (result) {
			return result;
		} else if (maybe_value || maybe_body) {
			return builder::let(let->varname(), value, body);
		} else {
			return {};
		}
	} else if (auto apply = input.as_apply()) {
		auto maybe_fn = visit_transform(apply->fn(), visitor);
		bool changed = !!maybe_fn;
		const auto& fn = maybe_fn ? *maybe_fn : apply->fn();

		std::vector<constr_t> args;
		for (const auto& arg : apply->args()) {
			auto maybe_arg = visit_transform(arg, visitor);
			changed = changed || maybe_arg;
			args.push_back(maybe_arg ? *maybe_arg : arg);
		}

		auto result = visitor.handle_apply(fn, args);
		if (result) {
			return result;
		} else if (changed) {
			return builder::apply(fn, std::move(args));
		} else {
			return {};
		}
	} else if (auto cast = input.as_cast()) {
		auto maybe_term = visit_transform(cast->term(), visitor);
		const auto& term = maybe_term ? *maybe_term : cast->term();

		auto maybe_typeterm = visit_transform(cast->typeterm(), visitor);
		const auto& typeterm = maybe_typeterm ? *maybe_typeterm : cast->typeterm();

		auto result = visitor.handle_cast(term, cast->kind(), typeterm);
		if (result) {
			return result;
		} else if (maybe_term || maybe_typeterm) {
			return builder::cast(term, cast->kind(), typeterm);
		} else {
			return {};
		}
	} else if (auto match_case = input.as_match()) {
		auto maybe_arg = visit_transform(match_case->arg(), visitor);
		const auto& arg = maybe_arg ? *maybe_arg : match_case->arg();

		visitor.push_local(
			nullptr,
			&arg,
			nullptr);
		auto maybe_restype = visit_transform(match_case->restype(), visitor);
		const auto& restype = maybe_restype ? *maybe_restype : match_case->restype();
		visitor.pop_local();

		bool changed = false;
		std::vector<match_branch_t> branches;
		for (const auto& branch : match_case->branches()) {
			for (std::size_t n = 0; n < branch.nargs; ++n) {
				visitor.push_local(nullptr, nullptr, nullptr);
			}
			auto maybe_expr = visit_transform(branch.expr, visitor);
			for (std::size_t n = 0; n < branch.nargs; ++n) {
				visitor.pop_local();
			}
			changed = changed || maybe_expr;
			const auto& expr = maybe_expr ? *maybe_expr : branch.expr;
			branches.push_back(match_branch_t{branch.constructor, branch.nargs, expr});
		}

		changed = changed || maybe_arg || maybe_restype;

		auto result = visitor.handle_case(restype, arg, branches);
		if (result) {
			return result;
		} else if (changed) {
			return builder::match(restype, arg, branches);
		} else {
			return {};
		}
	} else if (auto fix = input.as_fix()) {
		std::vector<fix_function_t> functions;
		for (const auto& function : fix->group()->functions) {
			// XXX: product at least correct signature for this function
			visitor.push_local(&function.name, nullptr, nullptr);
		}

		bool changed = false;
		for (const auto& function : fix->group()->functions) {
			std::vector<formal_arg_t> args;
			for (const auto& arg : function.args) {
				auto maybe_argtype = visit_transform(arg.type, visitor);
				changed = changed || maybe_argtype;
				const auto& argtype = maybe_argtype ? *maybe_argtype : arg.type;
				args.push_back({arg.name, argtype});
				visitor.push_local(arg.name ? &*arg.name : nullptr, &arg.type, nullptr);
			}
			auto maybe_restype = visit_transform(function.restype, visitor);
			const auto& restype = maybe_restype ? *maybe_restype : function.restype;

			auto maybe_body = visit_transform(function.body, visitor);
			const auto& body = maybe_body ? *maybe_body : function.body;
			changed = changed || maybe_restype || maybe_body;

			functions.push_back(fix_function_t{function.name, std::move(args), restype, body});
			for (const auto& arg : function.args) {
				(void) arg;
				visitor.pop_local();
			}
		}
		for (const auto& function : fix->group()->functions) {
			(void) function;
			visitor.pop_local();
		}
		std::shared_ptr<fix_group_t> new_group;
		if (changed) {
			new_group = std::make_shared<fix_group_t>();
			new_group->functions = std::move(functions);
		}

		const auto& group = changed ? new_group : fix->group();

		auto result = visitor.handle_fix(fix->index(), group);
		if (result) {
			return result;
		} else if (changed) {
			return builder::fix(fix->index(), std::move(new_group));
		} else {
			return {};
		}
	} else {
		throw std::logic_error("Unhandled constr kind");
	}
}

}  // namespace coqcic
