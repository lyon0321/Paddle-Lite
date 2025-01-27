// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lite/kernels/xpu/instance_norm_compute.h"
#include "lite/backends/xpu/xpu_header_sitter.h"
#include "lite/core/op_registry.h"

namespace paddle {
namespace lite {
namespace kernels {
namespace xpu {

template <typename T, PrecisionType PType>
void InstanceNormCompute<T, PType>::Run() {
  auto& param = this->template Param<param_t>();
  auto& ctx = this->ctx_->template As<XPUContext>();
  auto x_dims = param.x->dims();
  CHECK(x_dims.size() == 4 || x_dims.size() == 5)
      << "Not support x_dims_rank = " << x_dims.size();

  int n = x_dims[0];
  int c = x_dims[1];
  int h = x_dims[2];
  int w = x_dims[3];
  if (x_dims.size() == 5) {
    h = x_dims[2] * x_dims[3];
    w = x_dims[4];
  }

  float* xpu_scale = nullptr;
  XPUScratchPadGuard xpu_scale_guard =
      TargetWrapperXPU::MallocScratchPad(c * sizeof(float));
  if (param.scale == nullptr) {
    xpu_scale = reinterpret_cast<float*>(xpu_scale_guard->addr_);
    int ret = xdnn::constant<float>(ctx.GetRawContext(), xpu_scale, c, 1.0f);
    CHECK_EQ(ret, 0);
  }
  float* xpu_bias = nullptr;
  XPUScratchPadGuard xpu_bias_guard =
      TargetWrapperXPU::MallocScratchPad(c * sizeof(float));
  if (param.bias == nullptr) {
    xpu_bias = reinterpret_cast<float*>(xpu_bias_guard->addr_);
    int ret = xdnn::constant<float>(ctx.GetRawContext(), xpu_bias, c, 0.0f);
    CHECK_EQ(ret, 0);
  }
  int ret = xdnn::instance_norm<T>(
      ctx.GetRawContext(),
      param.x->template data<T>(),
      param.out->template mutable_data<T>(TARGET(kXPU)),
      n,
      c,
      h,
      w,
      param.epsilon,
      (param.scale == nullptr) ? xpu_scale
                               : param.scale->template data<float>(),
      (param.bias == nullptr) ? xpu_bias : param.bias->template data<float>(),
      param.saved_mean->template mutable_data<float>(TARGET(kXPU)),
      param.saved_variance->template mutable_data<float>(TARGET(kXPU)),
      true);
  CHECK_EQ(ret, 0);
}

}  // namespace xpu
}  // namespace kernels
}  // namespace lite
}  // namespace paddle

namespace xpu = paddle::lite::kernels::xpu;

using InstanceNorm_FP32 = xpu::InstanceNormCompute<float, PRECISION(kFloat)>;
using InstanceNorm_FP16 = xpu::InstanceNormCompute<float16, PRECISION(kFP16)>;

REGISTER_LITE_KERNEL(instance_norm, kXPU, kFloat, kNCHW, InstanceNorm_FP32, def)
    .BindInput("X", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindInput("Scale", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindInput("Bias", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindOutput("Y", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindOutput("SavedMean", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindOutput("SavedVariance", {LiteType::GetTensorTy(TARGET(kXPU))})
    .Finalize();

REGISTER_LITE_KERNEL(
    instance_norm, kXPU, kFP16, kNCHW, InstanceNorm_FP16, DISABLE_XPU1_fp16)
    .BindInput("X", {LiteType::GetTensorTy(TARGET(kXPU), PRECISION(kFP16))})
    .BindInput("Scale", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindInput("Bias", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindOutput("Y", {LiteType::GetTensorTy(TARGET(kXPU), PRECISION(kFP16))})
    .BindOutput("SavedMean", {LiteType::GetTensorTy(TARGET(kXPU))})
    .BindOutput("SavedVariance", {LiteType::GetTensorTy(TARGET(kXPU))})
    .Finalize();
