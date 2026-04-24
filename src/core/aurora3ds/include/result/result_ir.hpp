#pragma once
#include "./result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(ResultCode::IR, IR);

namespace ResultCode::IR {
	DEFINE_HORIZON_RESULT(NoDeviceConnected, 13, InvalidState, Status);
};
