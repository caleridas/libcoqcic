# coq cic representation in C++

This library provides a C++ representation of the
Calculus of Inductive constructions as used by coq.
The representation is similar but not identical to
the Coq internal data structures.

A small coq plugin in is provided to export Coq structures
as s-expressions, and the library supports parsing
and reading these s-expressions to reconstruct the
representation. (There is no good build integration for
this plugin as of yet).
