#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "coqcic/lazy_stack.h"

namespace coqcic {

/**
	\page de_bruijn_indices de Bruijn indices

	Coq maintains references to local variables by <em>de Bruijn indices</em>.
	Conceptually, every \ref constr_t needs to be interpreted in the context
	of a stack of local variables. The least recently (or "innermost") defined
	variable is at level 0, the next at level 1 etc.

	Various constructs can contain child constructs and modify the local stack
	of variables for interpretation by their child constructs. These are:

	- \ref constr_product
	- \ref constr_lambda
	- \ref constr_let
	- \ref constr_fix

	Each adds additional variables to the stack thereby shifting existing ones
	by one or more indices. The rules are explained in detail in the following
	sections.

	\section de_bruijn_product product de Bruijn indices

	A (dependent) product describes the relationship of a return value
	of a function object to its actual parameters.

	Resolution rules:

	- All (formal) argument types described by \ref coqcic::constr_product::args
	  "constr_product::args()" are evaluated in order starting with the first.
	- The first (formal) argument type is evaluated in the context of the current
	  variable stack. The result of this evaluation is pushed to the stack
	  and used for evaluation each subsequent formal argument(s).
	- This process is repeated for each (formal) argument, adding one new
	  element to the current variable stack each.
	- Finally, the \ref coqcic::constr_product::restype "result type" of the product is
	  interpreted in the context of all arguments pushed to the stack in order.

	Example:

	Given the a stack with variables of the following types:

	\code
	0 = F : Set -> Set
	1 = nat : Set
	\endcode

	The following product expression:
	\code
	'1 -> '1 -> '0 '2
	\endcode

	will resolve to an equivalent product of the followingtype

	\code
	nat -> F -> F nat
	\endcode


	\section de_bruijn_lambda lambda de Bruijn indices

	A lambda construct describes the function to compute an output given
	actual argumnts for each of its formal arguments.

	Resolution rules:

	- All (formal) argument types described by \ref coqcic::constr_lambda::args
	  "constr_lambda::args()" are evaluated in order starting with the first.
	- The first (formal) argument type is evaluated in the context of the current
	  variable stack. The result of this evaluation is pushed to the stack
	  and used for evaluation each subsequent formal argument(s).
	- This process is repeated for each (formal) argument, adding one new
	  element to the current variable stack each.
	- Finally, the \ref coqcic::constr_lambda::body "body" of the lambda is
	  interpreted in the context of all arguments pushed to the stack in order.

	Example:

	Given the following stack with variable types:

	\code
	0 = plus : nat -> nat -> nat
	1 = 21 : nat
	\endcode

	The following lambda expression:
	\code
	'1 => '1 => '0 '2 '2
	\endcode

	will resolve to a lambda expression of the following _type_
	(after resolving against types from the existing stack):

	\code
	nat => add => '0 '2 '2
	\endcode

	and to the following _value_ after resolving actual arguments:

	\code
	plus 21 21 = 42
	\endcode


	\section de_bruijn_let let de Bruijn indices

	A let constructs debscribes the binding of one expression to
	a variable, and evaluating the contained expression in context
	of this binding.

	Resolution rules:
	- The expression to be bound is evaluated in the context of the
	  current stack.
	- The evaluation result is then pushed as a new value to the stack.

	Example:

	Given the following stack:
	\code
	0 = 5 : nat
	1 = 12 : nat
	\endcode

	The following let expression:
	\code
	let a := '0 + '1 in '0 + '1
	\endcode

	Will resolve into the following:
	\code
	let a := 5 + 12 in a + 5 = (5 + 12) + 5 = 22
	\endcode


	\section de_bruijn_fix fix de Bruijn indices

	A fix construct describes a bundle of one or more mutually
	recursive fixpoint functions. A selected function out of this
	bundle is to be called when this is applied as an expression
	to arguments.

	Resolution rules:
	- Starting from the current stack, all functions in this
	  bundle are pushed to the stack in order of definition
	  (that is, function defined last is at top of stack).
	- Afterwards, each function is individually interpreted
	  in this amended context using its
	  \ref coqcic::fix_function_t::args "formal arguments" and
	  \ref coqcic::fix_function_t::restype "result type" in the
	  same way as \ref de_bruijn_lambda.
	- Peer functions within the bundle are therefore also
	  selected by de Bruijn index.

	This representation has the following "quirk": When building
	the local variable context, they types of functions pushed
	into the context need to be known. However, their formal
	argument list and result type are "shifted" to account for
	the indices of the functions themselves. "Formally" one
	function's types may therefore appear to depend on other
	functions, but there is never any real dependency.

	To solve this and obtain the pure signatures of each function,
	the user must apply negative \ref coqcic::constr_t::shift to both
	\ref coqcic::fix_function_t::args "formal arguments" as well
	\ref coqcic::fix_function_t::restype "result type" by the
	number of functions in this bundle. The member function
	\ref coqcic::fix_group_t::get_function_sig performs the
	necessary adaptation.
*/

}  // namespace coqcic
