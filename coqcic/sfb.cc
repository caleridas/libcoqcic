#include "coqcic/sfb.h"

namespace coqcic {

////////////////////////////////////////////////////////////////////////////////
// sfb

void
sfb_t::format(std::string& out) const
{
	repr_->format(out);
}

bool
sfb_t::operator==(const sfb_t& other) const noexcept
{
	return repr_ == other.repr_ || (*repr_ == *other.repr_);
}

std::string
sfb_t::debug_string() const
{
	std::string s;
	format(s);
	return s;
}

////////////////////////////////////////////////////////////////////////////////
// sfb_base

sfb_base::~sfb_base()
{
}
std::string
sfb_base::repr() const
{
	std::string s;
	format(s);
	return s;
}

////////////////////////////////////////////////////////////////////////////////
// sfb_definition

sfb_definition::~sfb_definition()
{
}

void
sfb_definition::format(std::string& out) const
{
	out += "Definition " + id_ + " : ";
	type_.format(out);
	out += " := ";
	value_.format(out);
	out += ".";
}

bool
sfb_definition::operator==(const sfb_base& other) const noexcept
{
	if (auto d = dynamic_cast<const sfb_definition*>(&other)) {
		return id_ == d->id_ && type_ == d->type_ && value_ == d->value_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// sfb_axiom

sfb_axiom::~sfb_axiom()
{
}

void
sfb_axiom::format(std::string& out) const
{
	out += "Definition " + id_ + " : ";
	type_.format(out);
	out += ".";
}

bool
sfb_axiom::operator==(const sfb_base& other) const noexcept
{
	if (auto a = dynamic_cast<const sfb_axiom*>(&other)) {
		return id_ == a->id_ && type_ == a->type_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// sfb_inductive

sfb_inductive::~sfb_inductive()
{
}

void
sfb_inductive::format(std::string& out) const
{
	bool first = true;
	for (const auto& ind : one_inductives_) {
		if (first) {
			out += "Inductive ";
		} else {
			out += "with ";
		}
		first = false;
		out += ind.id + " : ";
		ind.type.format(out);
		out += " :=\n";
		for (const auto& cons : ind.constructors) {
			out += " | " + cons.id + " : ";
			cons.type.format(out);
			out += "\n";
		}
	}
	out += ".";
}

bool
sfb_inductive::operator==(const sfb_base& other) const noexcept
{
	if (auto i = dynamic_cast<const sfb_inductive*>(&other)) {
		return one_inductives_ == i->one_inductives_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// modexpr

void
modexpr::format(std::string& out) const
{
	out += name;
	for (const auto& arg : args) {
		out += ' ';
		out += arg;
	}
}

////////////////////////////////////////////////////////////////////////////////
// module_body

void
module_body::format(std::string& out) const
{
	for (const auto& parameter : parameters_) {
		out += " (";
		out += parameter.first;
		out += " : ";
		parameter.second.format(out);
		out +=")";
	}

	repr_->format(out);
}

bool
module_body::operator==(const module_body& other) const noexcept
{
	return parameters_ == other.parameters_ && (repr_ == other.repr_ || *repr_ == *other.repr_);
}

std::string
module_body::debug_string() const
{
	std::string s;
	format(s);
	return s;
}

////////////////////////////////////////////////////////////////////////////////
// module_body_expr

module_body_repr::~module_body_repr()
{
}

////////////////////////////////////////////////////////////////////////////////
// module_body_algebraic_repr

module_body_algebraic_repr::~module_body_algebraic_repr()
{
}

void
module_body_algebraic_repr::format(std::string& out) const
{
	out += " := ";
	expr_.format(out);
}

bool
module_body_algebraic_repr::operator==(const module_body_repr& other) const noexcept
{
	if (auto r = dynamic_cast<const module_body_algebraic_repr*>(&other)) {
		return expr_ == r->expr_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// module_body_struct_repr

module_body_struct_repr::~module_body_struct_repr()
{
}

void
module_body_struct_repr::format(std::string& out) const
{
	out += ".\n";
	for (const auto& sfb : body_) {
		sfb.format(out);
		out += "\n";
	}
	out += "End";
}

bool
module_body_struct_repr::operator==(const module_body_repr& other) const noexcept
{
	if (auto r = dynamic_cast<const module_body_struct_repr*>(&other)) {
		return type_ == r->type_ && body_ == r->body_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// sfb_module

sfb_module::~sfb_module()
{
}

void
sfb_module::format(std::string& out) const
{
	out += "Module " + id_;
	body_.format(out);
	out += ".";
}

bool
sfb_module::operator==(const sfb_base& other) const noexcept
{
	if (auto m = dynamic_cast<const sfb_module*>(&other)) {
		return id_ == m->id_ && body_ == m->body_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// sfb_module_type

sfb_module_type::~sfb_module_type()
{
}

void
sfb_module_type::format(std::string& out) const
{
	out += "ModuleType " + id_;
	body_.format(out);
	out += ".";
}

bool
sfb_module_type::operator==(const sfb_base& other) const noexcept
{
	if (auto m = dynamic_cast<const sfb_module_type*>(&other)) {
		return id_ == m->id_ && body_ == m->body_;
	} else {
		return false;
	}
}


namespace builder {

sfb_t
definition(std::string id, constr_t type, constr_t value)
{
	return sfb_t(std::make_shared<sfb_definition>(std::move(id), std::move(type), std::move(value)));
}

sfb_t
axiom(std::string id, constr_t type)
{
	return sfb_t(std::make_shared<sfb_axiom>(std::move(id), std::move(type)));
}

sfb_t
inductive(std::vector<one_inductive_t> one_inductives)
{
	return sfb_t(std::make_shared<sfb_inductive>(std::move(one_inductives)));
}

sfb_t
module_def(std::string id, module_body body)
{
	return sfb_t(std::make_shared<sfb_module>(std::move(id), std::move(body)));
}

sfb_t
module_type_def(std::string id, module_body body)
{
	return sfb_t(std::make_shared<sfb_module_type>(std::move(id), std::move(body)));
}

}  // builder


}  // namespace coqcic
