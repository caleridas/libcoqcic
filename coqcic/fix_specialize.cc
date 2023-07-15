#include "coqcic/fix_specialize.h"

#include <iostream>

#include "coqcic/simpl.h"
#include "coqcic/visitor.h"

namespace coqcic {

namespace {

struct sym_fix_function {
	std::size_t index;
};

struct sym_spec_arg {
	std::size_t index;
};

struct sym_none {};

using sym = std::variant<sym_fix_function, sym_spec_arg, sym_none>;
using sym_stack = shared_stack<sym>;

inline std::ostream&
operator<<(std::ostream& os, const sym& s)
{
	if (auto fn = std::get_if<sym_fix_function>(&s)) {
		os << "fn" << fn->index;
	} else if (auto arg = std::get_if<sym_spec_arg>(&s)) {
		os << "arg" << arg->index;
	} else {
		os << "none";
	}
	return os;
}

struct fix_closure_state {
	// Number of formal arguments for each of the defined fixpoint functions.
	std::vector<std::size_t> arg_count;

	// Specialization call state for each individual fixpoint function. This
	// is none if we have not computed the state for some given function yet.
	std::vector<std::optional<fix_spec_info::function>> call_state;

	std::vector<std::size_t> newly_added;

	// Raised if an inconsistency in specialization is encountered during
	// closure computation.
	bool inconsistent = false;

	void
	add_call_state(
		std::size_t index,
		fix_spec_info::function info);
};

inline bool
operator==(const fix_spec_info::function& left, const fix_spec_info::function& right) noexcept
{
	return left.spec_args == right.spec_args;
}

inline bool
operator!=(const fix_spec_info::function& left, const fix_spec_info::function& right) noexcept
{
	return !(left == right);
}

inline std::ostream&
operator<<(std::ostream& os, const fix_spec_info::function& info)
{
	for (const auto& arg : info.spec_args) {
		if (arg) {
			os << *arg << " ";
		} else {
			os << "() ";
		}
	}
	return os;
}

void
fix_closure_state::add_call_state(
	std::size_t index,
	fix_spec_info::function info)
{
	if (call_state[index]) {
		if (*call_state[index] != info) {
			inconsistent = true;
		}
	} else {
		call_state[index] = std::move(info);
		newly_added.push_back(index);
	}
}

sym
visit_for_fix_closure_state(
	const constr& c,
	fix_closure_state& state,
	const sym_stack& locals,
	const sym_stack& apply_args)
{
	if (auto l = c.as_local()) {
		if (l->index() < locals.size()) {
			auto sym = locals.at(l->index());
			if (auto fn = std::get_if<sym_fix_function>(&sym)) {
				// One of our fixpoint functions. It must be called, and the
				// call must be saturated.
				std::size_t arg_count = state.arg_count[fn->index];
				if (apply_args.size() >= arg_count) {
					// Build arguments used in this call.
					fix_spec_info::function info;
					for (std::size_t n = 0; n < arg_count ; ++n) {
						auto spec = std::get_if<sym_spec_arg>(&apply_args.at(n));
						if (spec) {
							info.spec_args.push_back(std::optional<std::size_t>(spec->index));
						} else {
							info.spec_args.push_back(std::optional<std::size_t>());
						}
					}
					state.add_call_state(fn->index, std::move(info));
				} else {
					state.inconsistent = true;
				}
			}
			return sym;
		} else {
			return {sym_none{}};
		}
	} else if (auto global = c.as_global()) {
		(void) global;
		return {sym_none{}};
	} else if (auto builtin = c.as_builtin()) {
		(void) builtin;
		return {sym_none{}};
	} else if (auto product = c.as_product()) {
		// Products cannot lead to a recursive call (that would result in
		// universe inconsistency).
		// Should assert that no reference to any fixpoint function inside.
		(void) product;
		return {sym_none{}};
	} else if (auto lambda = c.as_lambda()) {
		// XXX: this is not 100% correct: if one of the argument
		// we are specializing over gets unbound/rebound, we will lose its value
		// and fail to specialize the recursive call. It is not clear we should
		// maybe "forbid" this pattern (e.g. by a "poison" sym value).
		visit_for_fix_closure_state(lambda->body(), state, locals.push({sym_none{}}), {});
		return {sym_none{}};
	} else if (auto let = c.as_let()) {
		auto value = visit_for_fix_closure_state(let->value(), state, locals, {});
		auto new_locals = locals.push(value);
		return visit_for_fix_closure_state(let->body(), state, new_locals, apply_args);
	} else if (auto apply = c.as_apply()) {
		auto new_apply_args = apply_args;
		std::vector<sym> args;
		// Add args to context, inside out.
		for (auto i = apply->args().rbegin(); i != apply->args().rend(); ++i) {
			const auto& arg = *i;
			new_apply_args = new_apply_args.push(visit_for_fix_closure_state(arg, state, locals, {}));
		}
		return visit_for_fix_closure_state(apply->fn(), state, locals, new_apply_args);
	} else if (auto cast = c.as_cast()) {
		// The type term cannot refer to fixpoint closure.
		return visit_for_fix_closure_state(cast->term(), state, locals, {});
	} else if (auto match_case = c.as_match()) {
		visit_for_fix_closure_state(match_case->arg(), state, locals, {});
		for (const auto& branch : match_case->branches()) {
			auto branch_locals = locals;
			for (std::size_t n = 0; n < branch.nargs; ++n) {
				branch_locals = branch_locals.push({sym_none{}});
			}
			visit_for_fix_closure_state(branch.expr, state, branch_locals, apply_args);
		}
		return {sym_none{}};
	} else if (auto fix = c.as_fix()) {
		// fixpoint in fixpoint? that is difficult but not necessarily
		// impossible to handle, however it is not clear whether this is legal
		// in CIC at all to call to the outer fix from inwards.
		// let's just assert that no external reference to our function group
		// is inside
		(void) fix;
		std::vector<std::size_t> extrefs = collect_external_references(c);
		for (std::size_t extref : extrefs) {
			if (extref >= locals.size()) {
				continue;
			}
			auto local = locals.at(extref);
			if (std::get_if<sym_fix_function>(&local)) {
				state.inconsistent = true;
			}
		}
		return {sym_none{}};
	} else {
		throw std::logic_error("Unhandled constr kind");
	}
}

void
vec_move_app(std::vector<std::size_t>& dst, std::vector<std::size_t>& src)
{
	dst.insert(dst.end(), std::begin(src), std::end(src));
	src.clear();
}

}  // namespace

std::optional<fix_spec_info>
compute_fix_specialization_closure(
	const fix_group& group,
	std::size_t fn_index,
	std::vector<std::optional<std::size_t>> seed_arg)
{
	fix_closure_state state;
	for (std::size_t n = 0; n < group.functions.size(); ++n) {
		state.arg_count.push_back(group.functions[n].args.size());
		state.call_state.push_back({});
	}

	state.add_call_state(fn_index, fix_spec_info::function{std::move(seed_arg)});

	std::vector<std::size_t> needs_processing;
	vec_move_app(needs_processing, state.newly_added);
	while (!needs_processing.empty()) {
		std::size_t index = needs_processing.back();
		needs_processing.pop_back();

		sym_stack locals;
		for (std::size_t n = 0; n < group.functions.size(); ++n) {
			locals = locals.push(sym_fix_function{n});
		}
		for (std::size_t n = 0; n < state.arg_count[index]; ++n) {
			const auto& arg = state.call_state[index]->spec_args[n];
			if (arg) {
				locals = locals.push(sym_spec_arg{*arg});
			} else {
				locals = locals.push(sym_none{});
			}
		}
		visit_for_fix_closure_state(group.functions[index].body, state, locals, {});

		vec_move_app(needs_processing, state.newly_added);
	}

	if (state.inconsistent) {
		return {};
	}

	fix_spec_info info;
	for (auto& fn_info : state.call_state) {
		if (!fn_info) {
			return {};
		}
		info.functions.push_back(std::move(*fn_info));
	}

	return {std::move(info)};
}

namespace {

struct replace_shift {
	std::size_t offset;
};

struct replace_subst {
	constr subst;
};

using replace = std::variant<replace_shift, replace_subst>;
using replace_stack = shared_stack<replace>;

class specialize_visitor final : public transform_visitor {
public:
	~specialize_visitor() override
	{
	}

	specialize_visitor(
		std::size_t depth,
		std::size_t extra_shift,
		replace_stack locals)
		: depth_(depth), extra_shift_(extra_shift), locals_(std::move(locals))
	{
	}

	void
	push_local(
		const std::string* name,
		const constr* type,
		const constr* value)
	{
		locals_ = locals_.push(replace_shift{extra_shift_});
		++depth_;
	}

	void
	pop_local()
	{
		locals_ = locals_.pop();
		--depth_;
	}

	std::optional<constr>
	handle_local(const std::string& name, std::size_t index) override
	{
		if (index >= locals_.size()) {
			return builder::local(name, index - extra_shift_);
		} else {
			auto repl = locals_.at(index);
			if (auto shift = std::get_if<replace_shift>(&repl)) {
				return builder::local(name, index + shift->offset - extra_shift_);
			} else if (auto subst = std::get_if<replace_subst>(&repl)) {
				return {subst->subst.shift(0, depth_)};
			} else {
				throw std::logic_error("Impossible replacement");
			}
		}
	}

	std::optional<constr>
	handle_apply(const constr& fn, const std::vector<constr>& args) override
	{
		if (auto lambda = fn.as_lambda()) {
			(void) lambda;
			// XXX: conditionally resolve apply/lambda
			return builder::apply(fn, args).simpl();
		} else {
			return {};
		}
	}

private:
	std::size_t depth_;
	std::size_t extra_shift_;
	replace_stack locals_;
};

}  // namespace

fix_group
apply_fix_specialization(
	const fix_group& group,
	const fix_spec_info& info,
	const std::vector<constr>& spec_args,
	const std::function<std::string(std::size_t)>& namegen)
{
	// Build the expressions for the replacement functions.
	replace_stack fix_locals;
	for (std::size_t fn_index = 0; fn_index < group.functions.size(); ++fn_index) {
		const auto& fn = group.functions[fn_index];
		std::size_t arg_count = 0;
		for (const auto& arg : info.functions[fn_index].spec_args) {
			if (!arg) {
				++arg_count;
			}
		}
		// Build inner call expression, with specialized arguments removed.
		auto expr = builder::local(namegen(fn_index), arg_count + group.functions.size() - fn_index);
		std::vector<constr> args;
		for (std::size_t n = 0; n < fn.args.size(); ++n) {
			if (!info.functions[fn_index].spec_args[n]) {
				args.push_back(builder::local(fn.args[n].name ? *fn.args[n].name : "_", fn.args.size() - n -1));
			}
		}
		if (!args.empty()) {
			expr = builder::apply(expr, std::move(args));
		}
		// Abstract over call expression, preserving all arguments this time.
		for (std::size_t n = 0; n < fn.args.size(); ++n) {
			std::size_t i = fn.args.size() - n - 1;
			expr = builder::lambda({{fn.args[i].name ? *fn.args[i].name : "_", fn.args[i].type}}, expr);
		}
		fix_locals = fix_locals.push(replace_subst{std::move(expr)});
	}

	fix_group new_group;

	// Now convert all function expressions in the group.
	for (std::size_t fn_index = 0; fn_index < group.functions.size(); ++fn_index) {
		const auto& fn = group.functions[fn_index];
		replace_stack locals = fix_locals;
		std::size_t depth = 0;
		std::size_t extra_shift = 0;

		std::vector<formal_arg> args;
		for (std::size_t i = 0; i < fn.args.size(); ++i) {
			if (info.functions[fn_index].spec_args[i]) {
				locals = locals.push(replace_subst{spec_args[*info.functions[fn_index].spec_args[i]]});
				extra_shift += 1;
			} else {
				auto argtype = visit_transform_simple<specialize_visitor>(fn.restype, depth, extra_shift, locals);
				args.insert(args.end(), formal_arg{fn.args[i].name, argtype});
				locals = locals.push(replace_shift{extra_shift});
				depth += 1;
			}
		}

		auto restype = visit_transform_simple<specialize_visitor>(fn.restype, depth, extra_shift, locals);

		auto body = visit_transform_simple<specialize_visitor>(fn.body, depth, extra_shift, locals);
		new_group.functions.push_back(
			fix_group::function{
				namegen(fn_index),
				std::move(args),
				std::move(restype),
				std::move(body)
			});
	}

	return new_group;
}

}  // namespace coqcic
