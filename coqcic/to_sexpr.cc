#include "coqcic/to_sexpr.h"

namespace coqcic {

sexpr name_to_sexpr(const std::optional<std::string>& name) {
	if (name) {
		return sexpr::make_compound(
			"Name",
			{
				sexpr::make_terminal(*name, 0)
			},
			0
		);
	} else {
		return sexpr::make_compound(
			"Anonymous",
			{
			},
			0
		);
	}
}

sexpr cast_kind_to_sexpr(constr_cast::kind_type kind) {
	switch (kind) {
		case constr_cast::kind_type::vm_cast: {
			return sexpr::make_terminal("VMcast", 0);
		}
		case constr_cast::kind_type::default_cast: {
			return sexpr::make_terminal("DEFAULTcast", 0);
		}
		case constr_cast::kind_type::revert_cast:  {
			return sexpr::make_terminal("REVERTcast", 0);
		}
		case constr_cast::kind_type::native_cast: {
			return sexpr::make_terminal("NATIVEcast", 0);
		}
		default: {
			throw std::logic_error("non-exhaustive pattern atching on constr_cast::kind_type");
		}
	}
}

sexpr fix_function_to_sexpr(const fix_function_t& fixfn) {
	return sexpr::make_compound(
		"Function",
		{
			name_to_sexpr(fixfn.name),
			constr_to_sexpr(builder::product(fixfn.args, fixfn.restype)),
			constr_to_sexpr(builder::lambda(fixfn.args, fixfn.body))
		},
		0
	);
}

sexpr constr_to_sexpr(const constr_t& constr) {
	return constr.visit(
		[](const auto& constr) -> sexpr {
			using T = std::decay_t<decltype(constr)>;
			if constexpr (std::is_same<T, constr_local>()) {
				return sexpr::make_compound(
					"Local",
					{
						sexpr::make_terminal(constr.name(), 0),
						sexpr::make_terminal(std::to_string(constr.index()), 0)
					},
					0
				);
			} else if constexpr (std::is_same<T, constr_global>()) {
				return sexpr::make_compound(
					"Global",
					{
						sexpr::make_terminal(constr.name(), 0)
					},
					0
				);
			} else if constexpr (std::is_same<T, constr_builtin>()) {
				return sexpr::make_compound(
					"Sort",
					{
						sexpr::make_terminal(constr.name(), 0)
					},
					0
				);
			} else if constexpr (std::is_same<T, constr_product>()) {
				auto e = constr_to_sexpr(constr.restype());
				for (std::size_t n = constr.args().size(); n; --n) {
					const auto& arg = constr.args()[n - 1];
					e = sexpr::make_compound(
						"Prod",
						{
							name_to_sexpr(arg.name),
							constr_to_sexpr(arg.type),
							std::move(e)
						},
						0
					);
				}
				return e;
			} else if constexpr (std::is_same<T, constr_lambda>()) {
				auto e = constr_to_sexpr(constr.body());
				for (std::size_t n = constr.args().size(); n; --n) {
					const auto& arg = constr.args()[n - 1];
					e = sexpr::make_compound(
						"Lambda",
						{
							name_to_sexpr(arg.name),
							constr_to_sexpr(arg.type),
							std::move(e)
						},
						0
					);
				}
				return e;
			} else if constexpr (std::is_same<T, constr_let>()) {
				return sexpr::make_compound(
					"LetIn",
					{
						name_to_sexpr(constr.varname()),
						constr_to_sexpr(constr.value()),
						constr_to_sexpr(constr.type()),
						constr_to_sexpr(constr.body())
					},
					0
				);
			} else if constexpr (std::is_same<T, constr_apply>()) {
				std::vector<sexpr> args;
				args.push_back(constr_to_sexpr(constr.fn()));
				for (const auto& arg : constr.args()) {
					args.push_back(constr_to_sexpr(arg));
				}
				return sexpr::make_compound("App", args, 0);
			} else if constexpr (std::is_same<T, constr_cast>()) {
				return sexpr::make_compound(
					"Cast",
					{
						constr_to_sexpr(constr.term()),
						cast_kind_to_sexpr(constr.kind()),
						constr_to_sexpr(constr.typeterm())
					},
					0
				);
			} else if constexpr (std::is_same<T, constr_match>()) {
				std::vector<sexpr> branches;
				for (const auto& branch : constr.branches()) {
					branches.push_back(
						sexpr::make_compound(
							"Branch",
							{
								sexpr::make_terminal(branch.constructor, 0),
								sexpr::make_terminal(std::to_string(branch.nargs), 0),
								constr_to_sexpr(branch.expr)
							},
							0
						)
					);
				}
				return sexpr::make_compound(
					"Case",
					{
						sexpr::make_terminal("1", 0),
						constr_to_sexpr(constr.casetype()),
						sexpr::make_compound(
							"Match",
							{
								constr_to_sexpr(constr.arg())
							},
							0
						),
						sexpr::make_compound(
							"Branches",
							branches,
							0
						)
					},
					0
				);
			} else if constexpr (std::is_same<T, constr_fix>()) {
				std::vector<sexpr> args;
				args.push_back(
					sexpr::make_terminal(std::to_string(constr.index()), 0)
				);

				for (const auto& fixfn : constr.group()->functions) {
					args.push_back(
						fix_function_to_sexpr(fixfn)
					);
				}

				return sexpr::make_compound("Fix", args, 0);
			} else {
				throw std::logic_error("non-exhaustice pattern matching on constr_t");
			}
		}
	);
}

}  // namespace coqcic
