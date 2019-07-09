//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <armnn/ArmNN.hpp>

namespace armnn_driver
{

armnn::TensorShape InferPreluOutputShape(const armnn::TensorShape& inputShape, const armnn::TensorShape& alphaShape);

} // namespace armnn_driver


