#pragma once
#include "./result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(ResultCode::GSP, GSP);

namespace ResultCode::GSP {
	DEFINE_HORIZON_RESULT(SuccessRegisterIRQ, 519, Success, Success);
};
