#ifndef _VKTSPVASMCOMPUTESHADERCASE_HPP
#define _VKTSPVASMCOMPUTESHADERCASE_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 *
 *//*!
 * \file
 * \brief Test Case Skeleton Based on Compute Shaders
 *//*--------------------------------------------------------------------*/

#include "vkPrograms.hpp"
#include "vktTestCase.hpp"

#include "vktSpvAsmComputeShaderTestUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

class SpvAsmComputeShaderCase : public TestCase
{
public:
						SpvAsmComputeShaderCase	(tcu::TestContext& testCtx, const char* name, const char* description, const ComputeShaderSpec& spec);
	void				initPrograms			(vk::SourceCollections& programCollection) const;
	TestInstance*		createInstance			(Context& ctx) const;

private:
	ComputeShaderSpec	m_shaderSpec;
};

enum ConvertTestFeatures
{
	CONVERT_TEST_USES_INT16,
	CONVERT_TEST_USES_INT64,
	CONVERT_TEST_USES_INT16_INT64,
};

class ConvertTestCase : public SpvAsmComputeShaderCase
{
public:
						ConvertTestCase	(tcu::TestContext& testCtx, const char* name, const char* description, const ComputeShaderSpec& spec, const ConvertTestFeatures features);
	TestInstance*		createInstance	(Context& ctx) const;
private:
	ComputeShaderSpec			m_shaderSpec;
	const ConvertTestFeatures	m_features;
};

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMCOMPUTESHADERCASE_HPP
