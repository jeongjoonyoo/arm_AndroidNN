//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "HalPolicy.hpp"

#include "Utils.hpp"

#include "../1.0/HalPolicy.hpp"

namespace
{
static std::vector<V1_0::OperationType> opsEquivalentInV10({
    V1_0::OperationType::ADD,
    V1_0::OperationType::AVERAGE_POOL_2D,
    V1_0::OperationType::CONCATENATION,
    V1_0::OperationType::CONV_2D,
    V1_0::OperationType::DEPTHWISE_CONV_2D,
    V1_0::OperationType::DEQUANTIZE,
    V1_0::OperationType::FLOOR,
    V1_0::OperationType::FULLY_CONNECTED,
    V1_0::OperationType::LOCAL_RESPONSE_NORMALIZATION,
    V1_0::OperationType::LOGISTIC,
    V1_0::OperationType::LSTM,
    V1_0::OperationType::L2_NORMALIZATION,
    V1_0::OperationType::L2_POOL_2D,
    V1_0::OperationType::MAX_POOL_2D,
    V1_0::OperationType::MUL,
    V1_0::OperationType::RELU,
    V1_0::OperationType::RELU1,
    V1_0::OperationType::RELU6,
    V1_0::OperationType::SOFTMAX,
    V1_0::OperationType::SPACE_TO_DEPTH,
    V1_0::OperationType::TANH,
    V1_0::OperationType::RESHAPE,
    V1_0::OperationType::RESIZE_BILINEAR,
});

bool CompliantWithVersion10(const V1_1::Operation & operation)
{
    std::vector<V1_0::OperationType>::iterator it;
    it = std::find(opsEquivalentInV10.begin(), opsEquivalentInV10.end(),
                   static_cast<V1_0::OperationType>(operation.type));

    if(it != opsEquivalentInV10.end())
    {
        return true;
    }
    return false;
}

V1_0::Operation ConvertOperationToVersion10(const V1_1::Operation & operation)
{
    V1_0::Operation v10Operation;
    v10Operation.type = static_cast<V1_0::OperationType>(operation.type);
    v10Operation.inputs = operation.inputs;
    v10Operation.outputs = operation.outputs;
    return v10Operation;
}
}

namespace armnn_driver
{
namespace hal_1_1
{

bool HalPolicy::ConvertOperation(const Operation& operation, const Model& model, ConversionData& data)
{
    if (CompliantWithVersion10(operation))
    {
        hal_1_0::HalPolicy::Operation v10Operation = ConvertOperationToVersion10(operation);
        hal_1_0::HalPolicy::Model v10Model = convertToV1_0(model);

        return hal_1_0::HalPolicy::ConvertOperation(v10Operation, v10Model, data);
    }
    else
    {
        switch (operation.type)
        {
            case V1_1::OperationType::DIV:
                return ConvertDiv(operation, model, data);
            case V1_1::OperationType::SUB:
                return ConvertSub(operation, model, data);
            case V1_1::OperationType::MEAN:
                return ConvertMean(operation, model, data);
            case V1_1::OperationType::PAD:
                return ConvertPad(operation, model, data);
            case V1_1::OperationType::SPACE_TO_BATCH_ND:
                return ConvertSpaceToBatchNd(operation, model, data);
            case V1_1::OperationType::SQUEEZE:
                return ConvertSqueeze(operation, model, data);
            case V1_1::OperationType::STRIDED_SLICE:
                return ConvertStridedSlice(operation, model, data);
            case V1_1::OperationType::TRANSPOSE:
                return ConvertTranspose(operation, model, data);
            case V1_1::OperationType::BATCH_TO_SPACE_ND:
                return ConvertBatchToSpaceNd(operation, model, data);
            default:
                return Fail("%s: Operation type %s not supported in ArmnnDriver",
                            __func__, toString(operation.type).c_str());
        }
    }
}

bool HalPolicy::ConvertDiv(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertDiv()");

    LayerInputHandle input0 = ConvertToLayerInputHandle<hal_1_1::HalPolicy>(operation, 0, model, data);
    LayerInputHandle input1 = ConvertToLayerInputHandle<hal_1_1::HalPolicy>(operation, 1, model, data);

    if (!input0.IsValid() || !input1.IsValid())
    {
        return Fail("%s: Operation has invalid inputs", __func__);
    }

    // The FuseActivation parameter is always the input index 2
    // and it should be optional
    ActivationFn activationFunction;
    if (!GetOptionalInputActivation<hal_1_1::HalPolicy>(operation, 2, activationFunction, model, data))
    {
        return Fail("%s: Operation has invalid inputs", __func__);
    }

    const Operand* output = GetOutputOperand<hal_1_1::HalPolicy>(operation, 0, model);
    if (!output)
    {
        return Fail("%s: Could not read output 0", __func__);
    }

    const armnn::TensorInfo& outputInfo = GetTensorInfoForOperand(*output);
    if (IsDynamicTensor(outputInfo))
    {
        return Fail("%s: Dynamic output tensors are not supported", __func__);
    }

    bool isSupported = false;
    FORWARD_LAYER_SUPPORT_FUNC(__func__,
                               IsDivisionSupported,
                               data.m_Backends,
                               isSupported,
                               input0.GetTensorInfo(),
                               input1.GetTensorInfo(),
                               outputInfo);
    if (!isSupported)
    {
        return false;
    }

    armnn::IConnectableLayer* const startLayer = data.m_Network->AddDivisionLayer();
    armnn::IConnectableLayer* const endLayer   = ProcessActivation(outputInfo, activationFunction, startLayer, data);

    const armnn::TensorInfo& inputTensorInfo0 = input0.GetTensorInfo();
    const armnn::TensorInfo& inputTensorInfo1 = input1.GetTensorInfo();

    if (endLayer)
    {
        BroadcastTensor(input0, input1, startLayer, *data.m_Network);
        return SetupAndTrackLayerOutputSlot<hal_1_1::HalPolicy>(operation, 0, *endLayer, model, data);
    }

    return Fail("%s: ProcessActivation failed", __func__);
}

bool HalPolicy::ConvertSub(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertSub()");
    return ::ConvertSub<hal_1_1::HalPolicy>(operation, model, data);
}

bool HalPolicy::ConvertMean(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertMean()");

    LayerInputHandle input = ConvertToLayerInputHandle<hal_1_1::HalPolicy>(operation, 0, model, data);
    if (!input.IsValid())
    {
        return Fail("%s: Operation has invalid inputs", __func__);
    }

    const Operand* output = GetOutputOperand<hal_1_1::HalPolicy>(operation, 0, model);
    if (!output)
    {
        return Fail("%s: Could not read output 0", __func__);
    }

    const armnn::TensorInfo& outputInfo = GetTensorInfoForOperand(*output);
    if (IsDynamicTensor(outputInfo))
    {
        return Fail("%s: Dynamic output tensors are not supported", __func__);
    }

    const Operand* axisOperand = GetInputOperand<hal_1_1::HalPolicy>(operation, 1, model);
    if (!axisOperand)
    {
        return Fail("%s: Could not read input 1", __func__);
    }

    std::vector<int32_t> axis;
    if (!GetTensorInt32Values<hal_1_1::HalPolicy>(*axisOperand, axis, model, data))
    {
        return Fail("%s: Input 1 has invalid values", __func__);
    }

    const armnn::TensorInfo& inputInfo = input.GetTensorInfo();

    // Convert the axis to unsigned int and remove duplicates.
    unsigned int rank = inputInfo.GetNumDimensions();
    std::set<unsigned int> uniqueAxis;
    std::transform(axis.begin(), axis.end(),
                   std::inserter(uniqueAxis, uniqueAxis.begin()),
                   [rank](int i) -> unsigned int { return (i + rank) % rank; });

    // Get the "keep dims" flag.
    int32_t keepDims = 0;
    if (!GetInputInt32<hal_1_1::HalPolicy>(operation, 2, keepDims, model, data))
    {
        return Fail("%s: Could not read input 2", __func__);
    }

    armnn::MeanDescriptor descriptor;
    descriptor.m_Axis.assign(uniqueAxis.begin(), uniqueAxis.end());
    descriptor.m_KeepDims = keepDims > 0;

    bool isSupported = false;
    FORWARD_LAYER_SUPPORT_FUNC(__func__,
                               IsMeanSupported,
                               data.m_Backends,
                               isSupported,
                               inputInfo,
                               outputInfo,
                               descriptor);
    if (!isSupported)
    {
        return false;
    }

    armnn::IConnectableLayer* const layer = data.m_Network->AddMeanLayer(descriptor);
    assert(layer != nullptr);
    input.Connect(layer->GetInputSlot(0));

    return SetupAndTrackLayerOutputSlot<hal_1_1::HalPolicy>(operation, 0, *layer, model, data);
}

bool HalPolicy::ConvertPad(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertPad()");
    return ::ConvertPad<hal_1_1::HalPolicy>(operation, model, data);
}

bool HalPolicy::ConvertSpaceToBatchNd(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertSpaceToBatchNd()");
    return ::ConvertSpaceToBatchNd<hal_1_1::HalPolicy>(operation, model, data);
}

bool HalPolicy::ConvertSqueeze(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertSqueeze()");

    LayerInputHandle input = ConvertToLayerInputHandle<hal_1_1::HalPolicy>(operation, 0, model, data);
    if (!input.IsValid())
    {
        return Fail("%s: Operation has invalid inputs", __func__);
    }

    const armnn::TensorInfo& inputInfo  = input.GetTensorInfo();
    unsigned int rank = inputInfo.GetNumDimensions();
    if (rank > 4)
    {
        Fail("%s: Inputs with rank greater than 4 are not supported", __func__);
    }

    const Operand* output = GetOutputOperand<hal_1_1::HalPolicy>(operation, 0, model);
    if (!output)
    {
        return Fail("%s: Could not read output 0", __func__);
    }

    if (IsDynamicTensor(GetTensorInfoForOperand(*output)))
    {
        return Fail("%s: Dynamic output tensors are not supported", __func__);
    }

    // NOTE: Axis is an optional parameter to SQUEEZE, therefore we do not want to generate a failure
    // if the operand index is out of bounds.
    const Operand* axisOperand = GetInputOperand<hal_1_1::HalPolicy>(operation, 1, model, false);

    const uint32_t dimensionSequence[] = { 0, 1, 2, 3 };

    std::vector<int32_t> axis;
    if (!axisOperand)
    {
        axis.assign(dimensionSequence,
                    dimensionSequence + rank);
    }
    else
    {
        GetTensorInt32Values<hal_1_1::HalPolicy>(*axisOperand, axis, model, data);
    }


    std::vector<uint32_t> outputDims;
    for (unsigned int i = 0; i < rank; i++)
    {
        bool skipSqueeze = (std::find(axis.begin(), axis.end(), i) == axis.end());
        auto currentDimension = inputInfo.GetShape()[i];
        if (skipSqueeze || currentDimension != 1)
        {
            outputDims.push_back(currentDimension);
        }
    }

    armnn::TensorShape outShape = armnn::TensorShape(outputDims.size(), outputDims.data());

    armnn::TensorInfo outputInfo = inputInfo;
    outputInfo.SetShape(outShape);

    armnn::ReshapeDescriptor reshapeDesc;
    reshapeDesc.m_TargetShape = outputInfo.GetShape();

    bool isSupported = false;
    FORWARD_LAYER_SUPPORT_FUNC(__func__,
                               IsReshapeSupported,
                               data.m_Backends,
                               isSupported,
                               inputInfo,
                               reshapeDesc);
    if (!isSupported)
    {
        return false;
    }

    armnn::IConnectableLayer* const layer = data.m_Network->AddReshapeLayer(reshapeDesc);
    assert(layer != nullptr);
    input.Connect(layer->GetInputSlot(0));

    return SetupAndTrackLayerOutputSlot<hal_1_1::HalPolicy>(operation, 0, *layer, model, data);
}

bool HalPolicy::ConvertStridedSlice(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertStridedSlice()");

    LayerInputHandle input = ConvertToLayerInputHandle<hal_1_1::HalPolicy>(operation, 0, model, data);
    if (!input.IsValid())
    {
        return Fail("%s: Operation has invalid inputs", __func__);
    }

    const armnn::TensorInfo& inputInfo = input.GetTensorInfo();
    unsigned int rank = inputInfo.GetNumDimensions();
    if (rank > 4)
    {
        Fail("%s: Inputs with rank greater than 4 are not supported", __func__);
    }

    const Operand* output = GetOutputOperand<hal_1_1::HalPolicy>(operation, 0, model);
    if (!output)
    {
        return Fail("%s: Could not read output 0", __func__);
    }

    const armnn::TensorInfo& outputInfo = GetTensorInfoForOperand(*output);
    if (IsDynamicTensor(outputInfo))
    {
        return Fail("%s: Dynamic output tensors are not supported", __func__);
    }

    const Operand* beginOperand   = GetInputOperand<hal_1_1::HalPolicy>(operation, 1, model);
    const Operand* endOperand     = GetInputOperand<hal_1_1::HalPolicy>(operation, 2, model);
    const Operand* stridesOperand = GetInputOperand<hal_1_1::HalPolicy>(operation, 3, model);

    std::vector<int32_t> beginValues;
    std::vector<int32_t> endValues;
    std::vector<int32_t> stridesValues;

    // The length of the beginOperand, endOperand and stridesOperand must be of a rank(input)
    auto ValidateInputOperands = [&] (const Operand& operand, std::vector<int32_t>& operandValues)
    {
        if (!GetTensorInt32Values<hal_1_1::HalPolicy>(operand, operandValues, model, data))
        {
            return false;
        }

        if (operandValues.size() != rank)
        {
            return false;
        }

        return true;
    };

    if (!ValidateInputOperands(*beginOperand, beginValues)
        || !ValidateInputOperands(*endOperand, endValues)
        || !ValidateInputOperands(*stridesOperand, stridesValues))
    {
        return Fail("%s: Operation has invalid input operand", __func__);
    }

    // Stride cannot have value '0'
    if (std::any_of(stridesValues.cbegin(), stridesValues.cend(), [](int32_t i){ return i == 0; }))
    {
        return Fail("%s: Stride must be non-zero value.", __func__);
    }

    armnn::StridedSliceDescriptor descriptor;
    descriptor.m_Begin.assign(beginValues.cbegin(), beginValues.cend());
    descriptor.m_End.assign(endValues.cbegin(), endValues.cend());
    descriptor.m_Stride.assign(stridesValues.cbegin(), stridesValues.cend());
    descriptor.m_DataLayout = armnn::DataLayout::NHWC;

    // Get the "begin_mask", "end_mask", and "shrink_axis_mask" flags
    if (!GetInputInt32<hal_1_1::HalPolicy>(operation, 4, descriptor.m_BeginMask, model, data) ||
        !GetInputInt32<hal_1_1::HalPolicy>(operation, 5, descriptor.m_EndMask, model, data) ||
        !GetInputInt32<hal_1_1::HalPolicy>(operation, 6, descriptor.m_ShrinkAxisMask, model, data))
    {
        return Fail("%s: Operation has invalid inputs", __func__);
    }

    bool isSupported = false;
    FORWARD_LAYER_SUPPORT_FUNC(__func__,
                               IsStridedSliceSupported,
                               data.m_Backends,
                               isSupported,
                               inputInfo,
                               outputInfo,
                               descriptor);
    if (!isSupported)
    {
        return false;
    }

    armnn::IConnectableLayer* const layer = data.m_Network->AddStridedSliceLayer(descriptor);
    assert(layer != nullptr);
    input.Connect(layer->GetInputSlot(0));

    return SetupAndTrackLayerOutputSlot<hal_1_1::HalPolicy>(operation, 0, *layer, model, data);
}

bool HalPolicy::ConvertTranspose(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertTranspose()");

    LayerInputHandle input = ConvertToLayerInputHandle<hal_1_1::HalPolicy>(operation, 0, model, data);
    if (!input.IsValid())
    {
        return Fail("%s: Operation has invalid inputs", __func__);
    }

    const armnn::TensorInfo& inputInfo = input.GetTensorInfo();
    unsigned int rank = inputInfo.GetNumDimensions();
    if (rank > 4)
    {
        Fail("%s: Inputs with rank greater than 4 are not supported", __func__);
    }

    // NOTE: Axis is an optional parameter to TRANSPOSE, therefore we do not want to generate a failure
    // if the operand index is out of bounds.
    const Operand* permOperand = GetInputOperand<hal_1_1::HalPolicy>(operation, 1, model, false);

    std::vector<int32_t> perm(rank);
    if (!permOperand)
    {
        // NOTE: If perm is not given, it is set to (n-1...0), where n is the rank of the tensor
        for (unsigned int i = rank; i > 0; i--)
        {
            perm[rank - i] = boost::numeric_cast<int> (i - 1);
        }
    }
    else
    {
        GetTensorInt32Values<hal_1_1::HalPolicy>(*permOperand, perm, model, data);
    }

    std::vector<uint32_t> outputDims(perm.begin(), perm.begin() + rank);

    auto permutationVector = armnn::PermutationVector(outputDims.data(), outputDims.size());
    if (!permutationVector.IsEqual(NHWCToArmNN)
        && !permutationVector.IsEqual(ArmNNToNHWC)
        && !permutationVector.IsEqual({ 3, 2, 0, 1 }))
    {
       return Fail("%s: Only [0, 3, 1, 2], [0, 2, 3, 1] and [3, 2, 0, 1] permutations are supported.", __func__);
    }

    armnn::PermuteDescriptor permuteDesc;
    permuteDesc.m_DimMappings = permutationVector;

    const Operand* output = GetOutputOperand<hal_1_1::HalPolicy>(operation, 0, model);
    if (!output)
    {
        return Fail("%s: Could not read output 0", __func__);
    }

    const armnn::TensorInfo& outputInfo = GetTensorInfoForOperand(*output);

    bool isSupported = false;
    FORWARD_LAYER_SUPPORT_FUNC(__func__,
                               IsPermuteSupported,
                               data.m_Backends,
                               isSupported,
                               inputInfo,
                               outputInfo,
                               permuteDesc);
    if (!isSupported)
    {
        return false;
    }

    armnn::IConnectableLayer* const layer = data.m_Network->AddPermuteLayer(permuteDesc);
    assert(layer != nullptr);
    input.Connect(layer->GetInputSlot(0));

    return SetupAndTrackLayerOutputSlot<hal_1_1::HalPolicy>(operation, 0, *layer, model, data);
}

bool HalPolicy::ConvertBatchToSpaceNd(const Operation& operation, const Model& model, ConversionData& data)
{
    ALOGV("hal_1_1::HalPolicy::ConvertBatchToSpaceNd()");
    return ::ConvertBatchToSpaceNd<hal_1_1::HalPolicy>(operation, model, data);
}

} // namespace hal_1_1
} // namespace armnn_driver
