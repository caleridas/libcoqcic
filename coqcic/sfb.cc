#include "coqcic/sfb.h"

namespace coqcic {

////////////////////////////////////////////////////////////////////////////////
// sfb

void
sfb::format(std::string& out) const
{
	repr_->format(out);
}

bool
sfb::operator==(const sfb& other) const noexcept
{
	return repr_ == other.repr_ || (*repr_ == *other.repr_);
}

std::string
sfb::debug_string() const
{
	std::string s;
	format(s);
	return s;
}

////////////////////////////////////////////////////////////////////////////////
// sfb_repr

sfb_repr::~sfb_repr()
{
}
std::string
sfb_repr::repr() const
{
	std::string s;
	format(s);
	return s;
}

////////////////////////////////////////////////////////////////////////////////
// definition_repr

definition_repr::~definition_repr()
{
}

void
definition_repr::format(std::string& out) const
{
	out += "Definition " + id_ + " : ";
	type_.format(out);
	out += " := ";
	value_.format(out);
	out += ".";
}

bool
definition_repr::operator==(const sfb_repr& other) const noexcept
{
	if (auto d = dynamic_cast<const definition_repr*>(&other)) {
		return id_ == d->id_ && type_ == d->type_ && value_ == d->value_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// axiom_repr

axiom_repr::~axiom_repr()
{
}

void
axiom_repr::format(std::string& out) const
{
	out += "Definition " + id_ + " : ";
	type_.format(out);
	out += ".";
}

bool
axiom_repr::operator==(const sfb_repr& other) const noexcept
{
	if (auto a = dynamic_cast<const axiom_repr*>(&other)) {
		return id_ == a->id_ && type_ == a->type_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// inductive_repr

inductive_repr::~inductive_repr()
{
}

void
inductive_repr::format(std::string& out) const
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
inductive_repr::operator==(const sfb_repr& other) const noexcept
{
	if (auto i = dynamic_cast<const inductive_repr*>(&other)) {
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
// module_repr

module_repr::~module_repr()
{
}

void
module_repr::format(std::string& out) const
{
	out += "Module " + id_;
	body_.format(out);
	out += ".";
}

bool
module_repr::operator==(const sfb_repr& other) const noexcept
{
	if (auto m = dynamic_cast<const module_repr*>(&other)) {
		return id_ == m->id_ && body_ == m->body_;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// module_type_repr

module_type_repr::~module_type_repr()
{
}

void
module_type_repr::format(std::string& out) const
{
	out += "ModuleType " + id_;
	body_.format(out);
	out += ".";
}

bool
module_type_repr::operator==(const sfb_repr& other) const noexcept
{
	if (auto m = dynamic_cast<const module_type_repr*>(&other)) {
		return id_ == m->id_ && body_ == m->body_;
	} else {
		return false;
	}
}


namespace builder {

sfb
definition(std::string id, constr type, constr value)
{
	return sfb(std::make_shared<definition_repr>(std::move(id), std::move(type), std::move(value)));
}

sfb
axiom(std::string id, constr type)
{
	return sfb(std::make_shared<axiom_repr>(std::move(id), std::move(type)));
}

sfb
inductive(std::vector<one_inductive> one_inductives)
{
	return sfb(std::make_shared<inductive_repr>(std::move(one_inductives)));
}

sfb
module_def(std::string id, module_body body)
{
	return sfb(std::make_shared<module_repr>(std::move(id), std::move(body)));
}

sfb
module_type_def(std::string id, module_body body)
{
	return sfb(std::make_shared<module_type_repr>(std::move(id), std::move(body)));
}

}  // builder


}  // namespace coqcic
