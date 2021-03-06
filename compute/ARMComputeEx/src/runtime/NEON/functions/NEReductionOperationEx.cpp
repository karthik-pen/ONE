/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright (c) 2017-2019 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "arm_compute/runtime/NEON/functions/NEReductionOperationEx.h"

#include "arm_compute/core/Helpers.h"
#include "arm_compute/runtime/NEON/NEScheduler.h"

namespace arm_compute
{
namespace
{
/** Define dimension to split the window
 *
 * @param[in] axis Reduction axis
 *
 * @return The dimension to split the window
 */
size_t reduction_window_split_dimension(unsigned int axis)
{
  switch (axis)
  {
    case 0:
      return Window::DimY;
    case 1:
    case 2:
    case 3:
      return Window::DimX;
    default:
      ARM_COMPUTE_ERROR("Unsupported reduction axis");
  }
}
} // namespace

NEReductionOperationEx::NEReductionOperationEx()
    : _reduction_kernel(), _fill_border_kernel(), _window_split(0), _reduction_axis()
{
}

Status NEReductionOperationEx::validate(const ITensorInfo *input, const ITensorInfo *output,
                                        unsigned int axis, ReduceOperation op)
{
  ARM_COMPUTE_RETURN_ON_ERROR(NEReductionOperationKernelEx::validate(input, output, axis, op));

  return Status{};
}

void NEReductionOperationEx::configure(ITensor *input, ITensor *output, unsigned int axis,
                                       ReduceOperation op)
{
  ARM_COMPUTE_ERROR_ON_NULLPTR(input, output);
  ARM_COMPUTE_ERROR_THROW_ON(
      NEReductionOperationEx::validate(input->info(), output->info(), axis, op));

  // Configure reduction kernel
  _reduction_kernel.configure(input, output, axis, op);
  _window_split = reduction_window_split_dimension(axis);
  _reduction_axis = axis;

  if (axis == 0)
  {
    // Configure fill border kernel
    const BorderSize fill_border_size = _reduction_kernel.border_size();
    PixelValue pixelValue;
    switch (op)
    {
      case ReduceOperation::MIN:
      {
        switch (input->info()->data_type())
        {
          case DataType::F32:
          {
            pixelValue = PixelValue(std::numeric_limits<float>::max());
            break;
          }
          case DataType::F16:
          {
            pixelValue = PixelValue(static_cast<half>(65504.0f));
            break;
          }
          case DataType::QASYMM8:
          {
            pixelValue =
                PixelValue(255, input->info()->data_type(), input->info()->quantization_info());
            break;
          }
          default:
          {
            ARM_COMPUTE_ERROR("Unsupported DataType");
          }
        }
        break;
      }
      case ReduceOperation::MAX:
      {
        switch (input->info()->data_type())
        {
          case DataType::F32:
          {
            pixelValue = PixelValue(-std::numeric_limits<float>::max());
            break;
          }
          case DataType::F16:
          {
            pixelValue = PixelValue(static_cast<half>(-65504.0f));
            break;
          }
          case DataType::QASYMM8:
          {
            pixelValue =
                PixelValue(0, input->info()->data_type(), input->info()->quantization_info());
            break;
          }
          default:
          {
            ARM_COMPUTE_ERROR("Unsupported DataType");
          }
        }
        break;
      }
      default:
        ARM_COMPUTE_ERROR("Reduction Operation unsupported");
    }
    _fill_border_kernel.configure(input, fill_border_size, BorderMode::CONSTANT, pixelValue);
  }
}

void NEReductionOperationEx::run()
{
  if (_reduction_axis == 0)
  {
    NEScheduler::get().schedule(&_fill_border_kernel, Window::DimY);
  }
  NEScheduler::get().schedule(&_reduction_kernel, _window_split);
}
} // namespace arm_compute
