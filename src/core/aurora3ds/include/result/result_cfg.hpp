#pragma once
#include "./result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(ResultCode::CFG, Config);

namespace ResultCode::CFG {
	DEFINE_HORIZON_RESULT(NotFound, 1018, WrongArgument, Permanent);
};
