#include "coqcic/normalize.h"

namespace coqcic {

namespace {

// Normalizes the given expression, that means:
// - "apply-of-apply" will be flattened into a single apply
// - "product-of-product" will be flattened into a single product
// - "lambda-of-lambda" will be flattened into a single lambda
std::optional<constr>
normalize_rec(const constr& input)
{
	if (input.as_local()) {
		return {};
	} else if (input.as_global()) {
		return {};
	} else if (input.as_builtin()) {
		return {};
	} else if (auto product = input.as_product()) {
		constr restype;
		bool changed = false;
		std::vector<formal_arg> args;

		while (product) {
			for (const auto& arg : product->args()) {
				auto maybe_argtype = normalize_rec(arg.type);
				changed = changed || maybe_argtype;
				args.push_back(maybe_argtype ? formal_arg{arg.name, *maybe_argtype} : arg);
			}
			restype = product->restype();
			product = restype.as_product();
			if (product) {
				changed = true;
			}
		}

		auto maybe_restype = normalize_rec(restype);
		const auto& new_restype = maybe_restype ? *maybe_restype : restype;
		changed = changed || maybe_restype;

		if (changed) {
			return builder::product(std::move(args), new_restype);
		} else {
			return {};
		}
	} else if (auto lambda = input.as_lambda()) {
		constr body;
		bool changed = false;
		std::vector<formal_arg> args;

		while (lambda) {
			for (const auto& arg : lambda->args()) {
				auto maybe_argtype = normalize_rec(arg.type);
				changed = changed || maybe_argtype;
				args.push_back(maybe_argtype ? formal_arg{arg.name, *maybe_argtype} : arg);
			}
			body = lambda->body();
			lambda = body.as_lambda();
			if (lambda) {
				changed = true;
			}
		}

		auto maybe_body = normalize_rec(body);
		const auto& new_body = maybe_body ? *maybe_body : body;
		changed = changed || maybe_body;

		if (changed) {
			return builder::lambda(std::move(args), new_body);
		} else {
			return {};
		}
	} else if (auto let = input.as_let()) {
		auto maybe_value = normalize_rec(let->value());
		const auto& value = maybe_value ? *maybe_value : let->value();

		auto maybe_body = normalize_rec(let->body());
		const auto& body = maybe_body ? *maybe_body : let->body();

		if (maybe_value || maybe_body) {
			return builder::let(let->varname(), value, body);
		} else {
			return {};
		}
	} else if (auto apply = input.as_apply()) {
		constr fn;
		bool changed = false;
		/* note that we will initially push args in reverse */
		std::vector<constr> args;

		while (apply) {
			for (auto i = apply->args().rbegin(); i != apply->args().rend(); ++i) {
				const auto& arg = *i;
				auto maybe_arg = normalize_rec(arg);
				changed = changed || maybe_arg;
				args.push_back(maybe_arg ? *maybe_arg : arg);
			}
			fn = apply->fn();
			apply = fn.as_apply();
			if (apply) {
				changed = true;
			}
		}

		auto maybe_fn = normalize_rec(fn);
		changed = changed || maybe_fn;
		const auto& new_fn = maybe_fn ? *maybe_fn : fn;
		if (changed) {
			std::reverse(args.begin(), args.end());
			return builder::apply(new_fn, std::move(args));
		} else {
			return {};
		}
	} else if (auto match_case = input.as_case()) {
		auto maybe_arg = normalize_rec(match_case->arg());
		const auto& arg = maybe_arg ? *maybe_arg : match_case->arg();

		auto maybe_restype = normalize_rec(match_case->restype());
		const auto& restype = maybe_restype ? *maybe_restype : match_case->restype();

		bool changed = false;
		std::vector<case_repr::branch> branches;
		for (const auto& branch : match_case->branches()) {
			auto maybe_expr = normalize_rec(branch.expr);
			changed = changed || maybe_expr;
			const auto& expr = maybe_expr ? *maybe_expr : branch.expr;
			branches.push_back(case_repr::branch{branch.constructor, branch.nargs, expr});
		}

		changed = changed || maybe_arg || maybe_restype;

		if (changed) {
			return builder::case_match(restype, arg, branches);
		} else {
			return {};
		}
	} else if (auto fix = input.as_fix()) {
		std::vector<fix_group::function> functions;

		bool changed = false;
		for (const auto& function : fix->group()->functions) {
			std::vector<formal_arg> args;
			for (const auto& arg : function.args) {
				auto maybe_argtype = normalize_rec(arg.type);
				changed = changed || maybe_argtype;
				const auto& argtype = maybe_argtype ? *maybe_argtype : arg.type;
				args.push_back({arg.name, argtype});
			}
			auto maybe_restype = normalize_rec(function.restype);
			const auto& restype = maybe_restype ? *maybe_restype : function.restype;

			auto maybe_body = normalize_rec(function.body);
			const auto& body = maybe_body ? *maybe_body : function.body;
			changed = changed || maybe_restype || maybe_body;

			functions.push_back(fix_group::function{function.name, std::move(args), restype, body});
		}
		std::shared_ptr<fix_group> new_group;
		if (changed) {
			new_group = std::make_shared<fix_group>();
			new_group->functions = std::move(functions);
		}

		if (changed) {
			return builder::fix(fix->index(), std::move(new_group));
		} else {
			return {};
		}
	} else {
		throw std::logic_error("Unhandled constr kind");
	}
}

}  // namespace

constr
normalize(const constr& expr)
{
	auto res = normalize_rec(expr);
	return res ? *res : expr;
}

}  // namespace coqcic
