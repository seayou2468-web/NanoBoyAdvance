#pragma once
#include "./result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(ResultCode::FND, FND);

namespace ResultCode::FND {
	DEFINE_HORIZON_RESULT(InvalidEnumValue, 1005, InvalidArgument, Permanent);
};
