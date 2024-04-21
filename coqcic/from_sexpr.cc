#include "coqcic/from_sexpr.h"

#include "coqcic/parse_sexpr.h"

namespace coqcic {

namespace {

from_sexpr_result<std::optional<std::string>>
argname_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();
		if (kind == "Name") {
			if (args.size() != 1 || !args[0].as_terminal()) {
				return from_sexpr_error {"Named argname requires single literal argument", &e};
			}
			return std::optional<std::string>(args[0].as_terminal()->value());
		} else if (kind == "Anonymous") {
			if (args.size() != 0) {
				return from_sexpr_error {"Anonymous argname does not allow an argument", &e};
			}
			return std::optional<std::string>(std::nullopt);
		} else {
			return from_sexpr_error {"Unknown kind of argname", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into argname", &e};
	}
}

from_sexpr_result<std::size_t>
uint_from_sexpr(const sexpr& e) {
	if (auto t = e.as_terminal()) {
		return std::stoi(t->value());
	} else {
		return from_sexpr_error {"Cannot parse non-terminal into integer", &e};
	}
}

from_sexpr_result<std::string>
string_from_sexpr(const sexpr& e) {
	if (auto t = e.as_terminal()) {
		return t->value();
	} else {
		return from_sexpr_error {"Cannot parse non-terminal into string", &e};
	}
}

from_sexpr_result<constr_t>
match_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();
		if (kind == "Match") {
			if (args.size() != 1) {
				return from_sexpr_error {"Match requires single argument", &e};
			}
			return constr_from_sexpr(args[0]);
		} else {
			return from_sexpr_error {"Unable to parse case match", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into match", &e};
	}
}

from_sexpr_result<match_branch_t>
branch_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "Branch") {
			if (args.size() != 3) {
				return from_sexpr_error {"Branch must have name and 2 arguments", &e};
			}
			auto consname = string_from_sexpr(args[0]);
			auto nargs = uint_from_sexpr(args[1]);
			auto expr = constr_from_sexpr(args[2]);
			if (!nargs) {
				return nargs.error();
			}
			if (!expr) {
				return expr.error();
			}
			return match_branch_t {consname.move_value(), nargs.move_value(), expr.move_value() };
		} else {
			return from_sexpr_error {"Unable to parse branch", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into branch", &e};
	}
}

from_sexpr_result<std::vector<match_branch_t>>
branches_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		auto kind = c->kind();
		auto args = c->args();
		if (kind == "Branches") {
			std::vector<match_branch_t> branches;
			for (const auto& arg : args) {
				auto branch = branch_from_sexpr(arg);
				if (!branch) {
					return branch.error();
				}
				branches.push_back(branch.move_value());
			}
			return std::move(branches);
		} else {
			return from_sexpr_error {"Unable to parse branches", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into branches", &e};
	}
}

from_sexpr_result<fix_function_t>
fixfunction_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();
		if (kind == "Function") {
			if (args.size() != 3) {
				return from_sexpr_error {"Fixfunction requires 3 arguments", &e};
			}
			auto name = argname_from_sexpr(args[0]);
			auto sigtype_parsed = constr_from_sexpr(args[1]);
			auto fndef_parsed = constr_from_sexpr(args[2]);
			if (!name) {
				return name.error();
			}
			if (!sigtype_parsed) {
				return sigtype_parsed.error();
			}
			if (!fndef_parsed) {
				return fndef_parsed.error();
			}
			std::string realname = name.value() ? std::move(*name.move_value()) : "_";

			auto sigtype = sigtype_parsed.move_value();
			auto fndef = fndef_parsed.move_value();

			std::vector<formal_arg_t> args;
			for (;;) {
				auto sigtype_prod = sigtype.as_product();
				auto fndef_lambda = fndef.as_lambda();

				if (!sigtype_prod || !fndef_lambda) {
					break;
				}

				std::size_t count = std::min(sigtype_prod->args().size(), fndef_lambda->args().size());
				args.insert(args.end(), fndef_lambda->args().begin(), fndef_lambda->args().begin() + count);

				std::vector<formal_arg_t> new_prod_args(sigtype_prod->args().begin() + count, sigtype_prod->args().end());
				std::vector<formal_arg_t> new_fndef_args(fndef_lambda->args().begin() + count, fndef_lambda->args().end());

				if (!new_prod_args.empty()) {
					sigtype = builder::product(std::move(new_prod_args), sigtype_prod->restype());
				} else {
					sigtype = sigtype_prod->restype();
				}

				if (!new_fndef_args.empty()) {
					fndef = builder::lambda(std::move(new_fndef_args), fndef_lambda->body());
				} else {
					fndef = fndef_lambda->body();
				}
			}

			return fix_function_t{std::move(realname), std::move(args), std::move(sigtype), std::move(fndef)};
		} else {
			return from_sexpr_error {"Unable to parse fixfunction", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into fixfunction", &e};
	}
}

}  // namespace

from_sexpr_result<sfb_t>
sfb_from_sexpr(const sexpr& e, std::shared_ptr<const fix_group_t>& last_fix);

from_sexpr_result<constr_t>
constr_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();
		if (kind == "Sort") {
			if (args.size() != 1 || !args[0].as_terminal()) {
				return from_sexpr_error {"Sort requires literal sort name as single argument", &e};
			}
			const auto& name = args[0].as_terminal()->value();
			if (name == "Prop") {
				return builder::builtin_prop();
			} else if (name == "Set") {
				return builder::builtin_set();
			} else if (name == "SProp") {
				return builder::builtin_sprop();
			} else if (name == "Type") {
				return builder::builtin_type();
			} else {
				return from_sexpr_error {"Unknown kind of sort", &args[0]};
			}
		} else if (kind == "Global") {
			if (args.size() != 1 || !args[0].as_terminal()) {
				return from_sexpr_error {"Global requires literal name as single argument", &e};
			}
			const auto& name = args[0].as_terminal()->value();
			return builder::global(name);
		} else if (kind == "Local") {
			if (args.size() != 2) {
				return from_sexpr_error {"Local requires literal name and index as arguments", &e};
			}
			auto name = string_from_sexpr(args[0]);
			if (!name) {
				return name.error();
			}
			auto index = uint_from_sexpr(args[1]);
			if (!index) {
				return index.error();
			}
			return builder::local(name.move_value(), index.move_value());
		} else if (kind == "Prod") {
			if (args.size() != 3) {
				return from_sexpr_error {"Product requires 3 arguments", &e};
			}
			auto argname = argname_from_sexpr(args[0]);
			auto argtype = constr_from_sexpr(args[1]);
			auto restype = constr_from_sexpr(args[2]);
			if (!argname) {
				return argname.error();
			}
			if (!argtype) {
				return argtype.error();
			}
			if (!restype) {
				return restype.error();
			}
			return builder::product({{argname.move_value(), argtype.move_value()}}, restype.move_value());
		} else if (kind == "Lambda") {
			if (args.size() != 3) {
				return from_sexpr_error {"Lambda requires 3 arguments", &e};
			}
			auto argname = argname_from_sexpr(args[0]);
			auto argtype = constr_from_sexpr(args[1]);
			auto body = constr_from_sexpr(args[2]);
			if (!argname) {
				return argname.error();
			}
			if (!argtype) {
				return argtype.error();
			}
			if (!body) {
				return body.error();
			}
			return builder::lambda({{argname.move_value(), argtype.move_value()}}, body.move_value());
		} else if (kind == "LetIn") {
			if (args.size() != 4) {
				return from_sexpr_error {"LetIn requires 4 arguments", &e};
			}
			auto name = argname_from_sexpr(args[0]);
			auto term = constr_from_sexpr(args[1]);
			auto termtype = constr_from_sexpr(args[2]);
			auto body = constr_from_sexpr(args[3]);
			if (!name) {
				return name.error();
			}
			if (!term) {
				return term.error();
			}
			if (!termtype) {
				return termtype.error();
			}
			if (!body) {
				return body.error();
			}
			return builder::let(name.move_value(), term.move_value(), termtype.move_value(), body.move_value());
		} else if (kind == "App") {
			if (args.size() < 2) {
				return from_sexpr_error {"Apply requires at least 2 arguments", &e};
			}
			auto fn = constr_from_sexpr(args[0]);
			if (!fn) {
				return fn.error();
			}
			std::vector<constr_t> app_args;
			for (std::size_t n = 1; n < args.size(); ++n) {
				auto arg = constr_from_sexpr(args[n]);
				if (!arg) {
					return arg.error();
				}
				app_args.push_back(arg.move_value());
			}
			return builder::apply(fn.move_value(), std::move(app_args));
		} else if (kind == "Cast") {
			if (args.size() != 3) {
				return from_sexpr_error {"Cast requires 3 arguments", &e};
			}
			auto term = constr_from_sexpr(args[0]);
			auto kind = string_from_sexpr(args[1]);
			auto typeterm = constr_from_sexpr(args[2]);
			if (!term) {
				return term.error();
			}
			if (!kind) {
				return kind.error();
			}
			if (!typeterm) {
				return typeterm.error();
			}
			constr_cast::kind_type kind_enum;
			if (kind.value() == "VMcast") {
				kind_enum = constr_cast::vm_cast;
			} else if (kind.value() == "DEFAULTcast") {
				kind_enum = constr_cast::default_cast;
			} else if (kind.value() == "REVERTcast") {
				kind_enum = constr_cast::revert_cast;
			} else if (kind.value() == "NATIVEcast") {
				kind_enum = constr_cast::native_cast;
			} else {
				return from_sexpr_error {"Unknown kind of cast", &e};
			}
			return builder::cast(term.move_value(), kind_enum, typeterm.move_value());
		} else if (kind == "Case") {
			if (args.size() != 4) {
				return from_sexpr_error {"Case requires at exactly 4 arguments", &e};
			}
			auto nargs = uint_from_sexpr(args[0]);
			if (!nargs) {
				return nargs.error();
			}
			auto casetype = constr_from_sexpr(args[1]);
			if (!casetype) {
				return casetype.error();
			}
			auto match = match_from_sexpr(args[2]);
			if (!match) {
				return match.error();
			}
			auto branches = branches_from_sexpr(args[3]);
			if (!branches) {
				return branches.error();
			}
			// XXX: casetype is not quite correct, needs one lambda abstraction removed
			return builder::match(casetype.move_value(), match.move_value(), branches.move_value());
		} else if (kind == "Fix") {
			if (args.size() < 2) {
				return from_sexpr_error {"Fix requires at least 2 arguments", &e};
			}
			auto index = uint_from_sexpr(args[0]);
			if (!index) {
				return index.error();
			}
			std::vector<fix_function_t> fns;
			for (std::size_t n = 1; n < args.size(); ++n) {
				const auto& arg = args[n];
				auto fixfn = fixfunction_from_sexpr(arg);
				if (!fixfn) {
					return fixfn.error();
				}
				fns.push_back(fixfn.move_value());
			}

			return builder::fix(index.move_value(), std::make_shared<fix_group_t>(fix_group_t{std::move(fns)}));
		} else {
			return from_sexpr_error {"Unhandled kind of constr:" + kind, &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into constr", &e};
	}
}

from_sexpr_result<constructor_t>
constructor_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "Constructor") {
			if (args.size() != 2) {
				return from_sexpr_error {"Constructor requires 2 arguments", &e};
			}

			auto id = string_from_sexpr(args[0]);
			if (!id) {
				return id.error();
			}
			auto type = constr_from_sexpr(args[1]);
			if (!type) {
				return type.error();
			}

			return constructor_t { id.move_value(), type.move_value() };
		} else {
			return from_sexpr_error {"Unhandled kind of constructor", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into constructor", &e};
	}
}

from_sexpr_result<one_inductive_t>
one_inductive_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "OneInductive") {
			if (args.size() < 2) {
				return from_sexpr_error {"Requires at least id and type for inductive", &e};
			}

			auto id = string_from_sexpr(args[0]);
			if (!id) {
				return id.error();
			}
			auto type = constr_from_sexpr(args[1]);
			if (!type) {
				return type.error();
			}
			std::vector<constructor_t> constructors;
			for (std::size_t n = 2; n < args.size(); ++n) {
				const auto& arg = args[n];
				auto cons = constructor_from_sexpr(arg);
				if (!cons) {
					return cons.error();
				}
				constructors.push_back(cons.move_value());
			}

			return one_inductive_t(id.move_value(), type.move_value(), std::move(constructors));
		} else {
			return from_sexpr_error {"Unhandled kind of sfb", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into one_inductive", &e};
	}
}

from_sexpr_result<modexpr>
modexpr_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "Apply") {
			if (args.size() != 2) {
				return from_sexpr_error {"Apply requires exactly 2 arguments", &e};
			}

			auto inner = modexpr_from_sexpr(args[0]);
			auto arg = string_from_sexpr(args[1]);

			if (!inner) {
				return inner.error();
			}
			if (!arg) {
				return arg.error();
			}

			modexpr e = inner.move_value();
			e.args.push_back(arg.move_value());
			return {std::move(e)};
		} else {
			return from_sexpr_error {"Unhandled kind of modexpr", &e};
		}
	} else if (auto t = e.as_terminal()) {
		return {modexpr { t->value(), {} }};
	} else {
		return from_sexpr_error {"Unhandled kind of sexpr", &e};
	}
}

from_sexpr_result<std::pair<std::vector<std::pair<std::string, modexpr>>, modexpr>>
functored_modexpr_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "Functor") {
			if (args.size() != 3) {
				return from_sexpr_error {"functor requires exactly 3 arguments", &e};
			}

			auto id = string_from_sexpr(args[0]);
			auto type = modexpr_from_sexpr(args[1]);
			auto inner = functored_modexpr_from_sexpr(args[2]);

			if (!id) {
				return id.error();
			}
			if (!type) {
				return type.error();
			}
			if (!inner) {
				return inner.error();
			}

			std::vector<std::pair<std::string, modexpr>> parameters;
			modexpr expr;
			std::tie(parameters, expr) = inner.move_value();
			parameters.emplace_back(id.move_value(), type.move_value());

			return std::pair<std::vector<std::pair<std::string, modexpr>>, modexpr>(std::move(parameters), std::move(expr));
		}
	}
	auto inner = modexpr_from_sexpr(e);
	if (!inner) {
		return inner.error();
	}

	return std::pair<std::vector<std::pair<std::string, modexpr>>, modexpr>({}, inner.move_value());
}

namespace {
using mod_functor_args_t = std::vector<std::pair<std::string, modexpr>>;
}  // namespace

from_sexpr_result<std::pair<mod_functor_args_t, std::vector<sfb_t>>>
modsig_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "Body") {
			std::shared_ptr<const fix_group_t> last_fix;
			std::pair<mod_functor_args_t, std::vector<sfb_t>> result;
			for (const auto& arg : args) {
				auto sfb = sfb_from_sexpr(arg, last_fix);
				if (!sfb) {
					return sfb.error();
				}
				result.second.push_back(sfb.move_value());
			}

			return {std::move(result)};
		} else if (kind == "Functor") {
			std::pair<mod_functor_args_t, std::vector<sfb_t>> result;
			auto name = string_from_sexpr(args[0]);
			auto type = functored_modexpr_from_sexpr(args[1]);
			auto inner = modsig_from_sexpr(args[2]);
			if (!name) {
				return name.error();
			}
			if (!type) {
				return type.error();
			}
			if (!inner) {
				return inner.error();
			}
			result = inner.move_value();
			result.first.emplace_back(name.move_value(), type.move_value().second);
			return {std::move(result)};
		} else {
			return from_sexpr_error {"Unhandled kind of modsig", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into modsig", &e};
	}
}

from_sexpr_result<std::optional<modexpr>>
optional_mod_type_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "Untyped") {
			return std::optional<modexpr>(std::nullopt);
		} else if (kind == "Typed") {
			if (args.size() != 1) {
				return from_sexpr_error {"Optional modtype requires exactly one argument", &e};
			}
			auto expr = functored_modexpr_from_sexpr(args[0]);
			if (!expr) {
				return expr.error();
			}

			return std::optional<modexpr>(expr.move_value().second);
		} else {
			return from_sexpr_error {"Unknown kind of module body", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into optional modtype", &e};
	}
}

from_sexpr_result<module_body>
module_body_from_sexpr(const sexpr& e) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "Algebraic") {
			if (args.size() != 1) {
				return from_sexpr_error {"Algebraic module requires exactly 1 argument", &e};
			}
			auto aexpr = functored_modexpr_from_sexpr(args[0]);
			if (!aexpr) {
				return aexpr.error();
			}

			std::vector<std::pair<std::string, modexpr>> parameters;
			modexpr expr;
			std::tie(parameters, expr) = aexpr.move_value();
			std::reverse(parameters.begin(), parameters.end());

			return module_body(std::move(parameters), std::make_shared<module_body_algebraic_repr>(std::move(expr)));
		} else if (kind == "Struct") {
			if (args.size() != 2) {
				return from_sexpr_error {"Struct module definition requires exactly 2 arguments", &e};
			}
			auto optional_type = optional_mod_type_from_sexpr(args[0]);
			auto modsig = modsig_from_sexpr(args[1]);
			if (!optional_type) {
				return optional_type.error();
			}
			if (!modsig) {
				return modsig.error();
			}

			std::vector<std::pair<std::string, modexpr>> parameters;
			std::vector<sfb_t> sfbs;
			std::tie(parameters, sfbs) = modsig.move_value();
			std::reverse(parameters.begin(), parameters.end());

			return module_body(std::move(parameters), std::make_shared<module_body_struct_repr>(optional_type.move_value(), std::move(sfbs)));
		} else {
			return from_sexpr_error {"Unknown kind of module body", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into module body", &e};
	}
}

from_sexpr_result<sfb_t>
sfb_from_sexpr(const sexpr& e, std::shared_ptr<const fix_group_t>& last_fix) {
	if (auto c = e.as_compound()) {
		const auto& kind = c->kind();
		const auto& args = c->args();

		if (kind == "Definition") {
			if (args.size() != 3) {
				return from_sexpr_error {"Definition requires 3 arguments", &e};
			}

			auto id = string_from_sexpr(args[0]);
			auto type = constr_from_sexpr(args[1]);
			auto value = constr_from_sexpr(args[2]);
			if (!id) {
				return id.error();
			}
			if (!type) {
				return type.error();
			}
			if (!value) {
				return value.error();
			}

			auto realvalue = value.move_value();
			// If this is a definition of a fixpoint, check if its group matches
			// that one of the last fixpoint defined. It if does, then reuse it.
			if (auto fix = realvalue.as_fix()) {
				if (last_fix) {
					if (*fix->group() == *last_fix) {
						realvalue = builder::fix(fix->index(), last_fix);
					} else {
						last_fix = fix->group();
					}
				} else {
					last_fix = fix->group();
				}
			}

			return builder::definition(id.move_value(), type.move_value(), std::move(realvalue));
		} else if (kind == "Axiom") {
			if (args.size() != 2) {
				return from_sexpr_error {"Axiom requires 2 arguments", &e};
			}

			auto id = string_from_sexpr(args[0]);
			auto type = constr_from_sexpr(args[1]);
			if (!id) {
				return id.error();
			}
			if (!type) {
				return type.error();
			}

			return builder::axiom(id.move_value(), type.move_value());
		} else if (kind == "Inductive") {
			if (args.size() < 1) {
				return from_sexpr_error {"Requires at least one inductive definition", &e};
			}

			std::vector<one_inductive_t> inds;
			for (const auto& arg : args) {
				auto ind = one_inductive_from_sexpr(arg);
				if (!ind) {
					return ind.error();
				}
				inds.push_back(ind.move_value());
			}

			return builder::inductive(std::move(inds));
		} else if (kind == "Module") {
			if (args.size() != 2) {
				return from_sexpr_error {"Module requires exactly two arguments", &e};
			}

			auto id = string_from_sexpr(args[0]);
			auto body = module_body_from_sexpr(args[1]);
			if (!id) {
				return id.error();
			}
			if (!body) {
				return body.error();
			}

			return builder::module_def(id.move_value(), body.move_value());
		} else if (kind == "ModuleType") {
			if (args.size() != 2) {
				return from_sexpr_error {"ModuleType requires exactly two arguments", &e};
			}
			auto id = string_from_sexpr(args[0]);
			auto modsig = modsig_from_sexpr(args[1]);

			if (!id) {
				return id.error();
			}
			if (!modsig) {
				return modsig.error();
			}

			std::vector<std::pair<std::string, modexpr>> parameters;
			std::vector<sfb_t> sfbs;
			std::tie(parameters, sfbs) = modsig.move_value();
			std::reverse(parameters.begin(), parameters.end());

			return builder::module_type_def(
				id.move_value(),
				module_body(std::move(parameters), std::make_shared<module_body_struct_repr>(std::nullopt, std::move(sfbs))));
		} else {
			return from_sexpr_error {"Unhandled kind of sfb", &e};
		}
	} else {
		return from_sexpr_error {"Cannot parse terminal into sfb", &e};
	}
}

from_sexpr_result<sfb_t>
sfb_from_sexpr(const sexpr& e) {
	std::shared_ptr<const fix_group_t> tmp;
	return sfb_from_sexpr(e, tmp);
}

from_sexpr_str_result<constr_t>
constr_from_sexpr_str(const std::string& str) {
	auto e = parse_sexpr(str);
	if (!e) {
		return from_sexpr_str_error {e.error().description, e.error().location};
	}

	auto c = constr_from_sexpr(e.value());
	if (!c) {
		return from_sexpr_str_error {
			c.error().description,
			c.error().context ? c.error().context->location() : 0
		};
	}

	return c.move_value();
}

from_sexpr_str_result<sfb_t>
sfb_from_sexpr_str(const std::string& str) {
	auto e = parse_sexpr(str);
	if (!e) {
		return from_sexpr_str_error {e.error().description, e.error().location};
	}

	auto s = sfb_from_sexpr(e.value());
	if (!s) {
		return from_sexpr_str_error {
			s.error().description,
			s.error().context ? s.error().context->location() : 0
		};
	}

	return s.move_value();
}

}  // namespace coqcic
