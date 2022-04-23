#ifndef COQCIC_PARSE_RESULT_H
#define COQCIC_PARSE_RESULT_H

#include "coqcic/constr.h"

namespace coqcic {

template<typename ResultType, typename ErrorType>
class parse_result {
public:
	parse_result(ResultType value)
		: repr_(std::move(value))
	{
	}

	parse_result(ErrorType error)
		: repr_(std::move(error))
	{
	}

	inline operator bool() const noexcept
	{
		return !!std::get_if<ResultType>(&repr_);
	}

	inline const ResultType& value() const noexcept
	{
		return std::get<ResultType>(repr_);
	}

	inline ResultType move_value() noexcept
	{
		ResultType value = std::move(std::get<ResultType>(repr_));
		return value;
	}

	inline const ErrorType& error() const noexcept
	{
		return std::get<ErrorType>(repr_);
	}

private:
	std::variant<ResultType, ErrorType> repr_;
};

}  // namespace coqcic

#endif  // COQCIC_PARSE_RESULT_H
