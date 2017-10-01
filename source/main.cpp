/*
* Vulkan Dungeon Crawler
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <omp.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanBuffer.hpp"
#include "VulkanTexture.hpp"
#include "VulkanModel.hpp"
#include "frustum.hpp"

#include "generator/Dungeon.h"

#define ENABLE_VALIDATION false

// Offsets for cell rendering vertex buffer
#define INDEX_OFFSET_FLOOR 0
#define INDEX_OFFSET_WALL_NORTH 6
#define INDEX_OFFSET_WALL_SOUTH 12
#define INDEX_OFFSET_WALL_WEST 18
#define INDEX_OFFSET_WALL_EAST 24
#define INDEX_OFFSET_CEILING 30

struct TextureSet {
	vks::Texture2DArray color;
	void load(std::string name, vks::VulkanDevice *device, VkQueue queue) {
		std::string folder("./../data/texturesets/");
		color.loadFromFile(folder + name + ".ktx", VK_FORMAT_R8G8B8A8_UNORM, device, queue);
	}
};

class VulkanExample : public VulkanExampleBase
{
public:
	bool topdown = false;

	uint32_t cellsVisible;
	uint32_t maxDrawDistance = 16;

	vks::Frustum frustum;

	vks::VertexLayout vertexLayout = vks::VertexLayout({
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_NORMAL,	// Actually UV with 3 components
	});

	// Buffers for building a dungeon cell
	vks::Buffer vertexBuffer, indexBuffer;

	struct TextureSets {
		TextureSet default;
	} textureSets;

	struct {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
	} uboVS, uboOffscreenVS;

	struct Light {
		glm::vec4 position;
		glm::vec3 color;
		float radius;
	};

	struct {
		Light lights[6];
		glm::vec4 viewPos;
	} uboFragmentLights;

	struct {
		vks::Buffer vsFullScreen;
		vks::Buffer vsOffscreen;
		vks::Buffer fsLights;
	} uniformBuffers;

	struct {
		VkPipeline composition;
		VkPipeline offscreen;
	} pipelines;

	struct {
		VkPipelineLayout composition;
		VkPipelineLayout offscreen;
	} pipelineLayouts;

	struct {
		VkDescriptorSet model;
		VkDescriptorSet floor;
	} descriptorSets;

	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
		VkFormat format;
	};
	struct FrameBuffer {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment position, normal, albedo;
		FrameBufferAttachment depth;
		VkRenderPass renderPass;
		VkSampler sampler;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkSemaphore semaphore;
	} deferredPass;

	dungeongenerator::Dungeon *dungeon;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Vulkan Dungeon Crawler";
		enableTextOverlay = true;

		srand(time(NULL));

		dungeon = new dungeongenerator::Dungeon(64, 64);
		dungeon->generateRooms();
		dungeon->generateWalls();
		dungeon->generateDoors();

		dungeongenerator::BspPartition* startingRoom = dungeon->getRandomRoom();

		camera.type = Camera::CameraType::firstperson;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 1024.0f);

		if (topdown) {
			camera.setRotation(glm::vec3(-25.0f, 0.0f, 0.0f));
			camera.setTranslation(glm::vec3(0.0f, 10.0f, 0.0f));
		}
		else {
			camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
			camera.setTranslation(glm::vec3(-(startingRoom->centerX - dungeon->width * 0.5f), 0.5f, -(startingRoom->centerY - dungeon->width * 0.5f)));
		}

		camera.movementSpeed *= 4.0f;
	}

	~VulkanExample()
	{
		vkDestroySampler(device, deferredPass.sampler, nullptr);
		vkDestroyImageView(device, deferredPass.position.view, nullptr);
		vkDestroyImage(device, deferredPass.position.image, nullptr);
		vkFreeMemory(device, deferredPass.position.mem, nullptr);
		vkDestroyImageView(device, deferredPass.normal.view, nullptr);
		vkDestroyImage(device, deferredPass.normal.image, nullptr);
		vkFreeMemory(device, deferredPass.normal.mem, nullptr);
		vkDestroyImageView(device, deferredPass.albedo.view, nullptr);
		vkDestroyImage(device, deferredPass.albedo.image, nullptr);
		vkFreeMemory(device, deferredPass.albedo.mem, nullptr);
		vkDestroyImageView(device, deferredPass.depth.view, nullptr);
		vkDestroyImage(device, deferredPass.depth.image, nullptr);
		vkFreeMemory(device, deferredPass.depth.mem, nullptr);
		vkDestroyFramebuffer(device, deferredPass.frameBuffer, nullptr);
		vkDestroyPipeline(device, pipelines.composition, nullptr);
		vkDestroyPipeline(device, pipelines.offscreen, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.composition, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.offscreen, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		uniformBuffers.vsOffscreen.destroy();
		uniformBuffers.vsFullScreen.destroy();
		uniformBuffers.fsLights.destroy();
		vkFreeCommandBuffers(device, cmdPool, 1, &deferredPass.commandBuffer);
		vkDestroyRenderPass(device, deferredPass.renderPass, nullptr);
		vkDestroySemaphore(device, deferredPass.semaphore, nullptr);
	}

	// Enable physical device features required for this example				
	virtual void getEnabledFeatures()
	{
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
	};

	/*
		Build vertex (and Index) buffer used as a building block for dungeon cells
	*/
	void buildVertexBuffers()
	{
		struct Vertex {
			float position[3];
			float normal[3];
			float uv[3];
		};
		const float d = 0.5f;
		const float h = -1.0f;
		const float z = 0.0f;

		/* vl = -x, vm = +x*/

		std::vector<Vertex> vertices = {
			/* Floor */
			{ { -d, z, -d },{ 0.0f, -1.0f,  0.0f },{ 0.0f, 0.0f, 0.0f } },
			{ { -d, z,  d },{ 0.0f, -1.0f,  0.0f },{ 0.0f, 1.0f, 0.0f } },
			{ {  d, z,  d },{ 0.0f, -1.0f,  0.0f },{ 1.0f, 1.0f, 0.0f } },
			{ {  d, z,  d },{ 0.0f, -1.0f,  0.0f },{ 1.0f, 1.0f, 0.0f } },
			{ {  d, z, -d },{ 0.0f, -1.0f,  0.0f },{ 1.0f, 0.0f, 0.0f } },
			{ { -d, z, -d },{ 0.0f, -1.0f,  0.0f },{ 0.0f, 0.0f, 0.0f } },
			/* Wall (North) */
			{ { -d, z, -d },{ 0.0f, 0.0f, 1.0f },{ 1.0f, 0.0f, 1.0f } },
			{ {  d, z, -d },{ 0.0f, 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -d, h, -d },{ 0.0f, 0.0f, 1.0f },{ 1.0f, 1.0f, 1.0f } },
			{ {  d, z, -d },{ 0.0f, 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ {  d, h, -d },{ 0.0f, 0.0f, 1.0f },{ 0.0f, 1.0f, 1.0f } },
			{ { -d, h, -d },{ 0.0f, 0.0f, 1.0f },{ 1.0f, 1.0f, 1.0f } },
			/* Wall (South) */
			{ { -d, h,  d },{ 0.0f, 0.0f, -1.0f },{ 0.0f, 1.0f, 1.0f } },
			{ {  d, z,  d },{ 0.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, 1.0f } },
			{ { -d, z,  d },{ 0.0f, 0.0f, -1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -d, h,  d },{ 0.0f, 0.0f, -1.0f },{ 0.0f, 1.0f, 1.0f } },
			{ {  d, h,  d },{ 0.0f, 0.0f, -1.0f },{ 1.0f, 1.0f, 1.0f } },
			{ {  d, z,  d },{ 0.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, 1.0f } },
			/* Wall (West) */
			{ { -d, h,  d },{ 1.0f, 0.0f,  0.0f },{ 1.0f, 1.0f, 1.0f } },
			{ { -d, z, -d },{ 1.0f, 0.0f,  0.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -d, h, -d },{ 1.0f, 0.0f,  0.0f },{ 0.0f, 1.0f, 1.0f } },
			{ { -d, h,  d },{ 1.0f, 0.0f,  0.0f },{ 1.0f, 1.0f, 1.0f } },
			{ { -d, z,  d },{ 1.0f, 0.0f,  0.0f },{ 1.0f, 0.0f, 1.0f } },
			{ { -d, z, -d },{ 1.0f, 0.0f,  0.0f },{ 0.0f, 0.0f, 1.0f } },
			/* Wall (East) */
			{ {  d, h, -d },{ -1.0f, 0.0f,  0.0f },{ 1.0f, 1.0f, 1.0f } },
			{ {  d, z, -d },{ -1.0f, 0.0f,  0.0f },{ 1.0f, 0.0f, 1.0f } },
			{ {  d, h,  d },{ -1.0f, 0.0f,  0.0f },{ 0.0f, 1.0f, 1.0f } },
			{ {  d, z, -d },{ -1.0f, 0.0f,  0.0f },{ 1.0f, 0.0f, 1.0f } },
			{ {  d, z,  d },{ -1.0f, 0.0f,  0.0f },{ 0.0f, 0.0f, 1.0f } },
			{ {  d, h,  d },{ -1.0f, 0.0f,  0.0f },{ 0.0f, 1.0f, 1.0f } },
			/* Ceiling */
			{ { -d, h, -d },{ 0.0f, -1.0f,  0.0f },{ 0.0f, 0.0f, 2.0f } },
			{ { -d, h,  d },{ 0.0f, -1.0f,  0.0f },{ 0.0f, 1.0f, 2.0f } },
			{ {  d, h,  d },{ 0.0f, -1.0f,  0.0f },{ 1.0f, 1.0f, 2.0f } },
			{ {  d, h,  d },{ 0.0f, -1.0f,  0.0f },{ 1.0f, 1.0f, 2.0f } },
			{ {  d, h, -d },{ 0.0f, -1.0f,  0.0f },{ 1.0f, 0.0f, 2.0f } },
			{ { -d, h, -d },{ 0.0f, -1.0f,  0.0f },{ 0.0f, 0.0f, 2.0f } },
		};
		std::vector<uint32_t> indices(6 * 6);
		uint32_t n = 0;
		std::generate(indices.begin(), indices.end(), [&n] { return n++; });

		const VkDeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);
		const VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

		vks::Buffer stagingBuffer;

		{
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&stagingBuffer,
				vertexBufferSize,
				vertices.data()));

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&vertexBuffer,
				vertexBufferSize));

			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy copyRegion = {};
			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, vertexBuffer.buffer, 1, &copyRegion);
			vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

			stagingBuffer.destroy();
		}

		{
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&stagingBuffer,
				indexBufferSize,
				indices.data()));

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&indexBuffer,
				indexBufferSize));

			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy copyRegion = {};
			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, indexBuffer.buffer, 1, &copyRegion);
			vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

			stagingBuffer.destroy();
		}
	}

	/*
		Create a frame buffer attachment
	*/
	void createAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment *attachment)
	{
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;

		attachment->format = format;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image = vks::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = format;
		image.extent.width = deferredPass.width;
		image.extent.height = deferredPass.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &attachment->image));
		vkGetImageMemoryRequirements(device, attachment->image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &attachment->mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, attachment->image, attachment->mem, 0));

		VkImageViewCreateInfo imageView = vks::initializers::imageViewCreateInfo();
		imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageView.format = format;
		imageView.subresourceRange = {};
		imageView.subresourceRange.aspectMask = aspectMask;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.image = attachment->image;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageView, nullptr, &attachment->view));
	}

	/*
		Prepare a new framebuffer and attachments for offscreen rendering (G-Buffer)
	*/
	void preparedeferredPassfer()
	{
		deferredPass.width = width;
		deferredPass.height = height;

		// Color attachments
		// (World space) Positions
		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &deferredPass.position);
		// (World space) Normals
		createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &deferredPass.normal);
		// Albedo (color)
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &deferredPass.albedo);

		// Depth attachment
		// Find a suitable depth format
		VkFormat attDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat);
		assert(validDepthFormat);
		createAttachment(attDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &deferredPass.depth);

		// Set up separate renderpass with references to the color and depth attachments
		std::array<VkAttachmentDescription, 4> attachmentDescs = {};
		// Init attachment properties
		for (uint32_t i = 0; i < 4; ++i) {
			attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			if (i == 3) {
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else {
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
		}

		// Formats
		attachmentDescs[0].format = deferredPass.position.format;
		attachmentDescs[1].format = deferredPass.normal.format;
		attachmentDescs[2].format = deferredPass.albedo.format;
		attachmentDescs[3].format = deferredPass.depth.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 3;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		subpass.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for attachment layput transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pAttachments = attachmentDescs.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &deferredPass.renderPass));

		std::array<VkImageView, 4> attachments;
		attachments[0] = deferredPass.position.view;
		attachments[1] = deferredPass.normal.view;
		attachments[2] = deferredPass.albedo.view;
		attachments[3] = deferredPass.depth.view;

		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = deferredPass.renderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = deferredPass.width;
		fbufCreateInfo.height = deferredPass.height;
		fbufCreateInfo.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &deferredPass.frameBuffer));

		// Create sampler to sample from the color attachments
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_NEAREST;
		sampler.minFilter = VK_FILTER_NEAREST;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &deferredPass.sampler));
	}

	/*
		Pre-build secondary command buffers for each cell
	*/
	// TODO: Move into cell class
	void generateCellCommandBuffers() {
		for (uint32_t x = 0; x < dungeon->width; x++) {
			for (uint32_t y = 0; y < dungeon->height; y++) {
				dungeongenerator::Cell *cell = dungeon->cells[x][y];

				glm::vec3 pos = glm::vec3(x * 1.0f - dungeon->width * 0.5f, 0.0, y * 1.0f - dungeon->height * 0.5f);

				if (cell->type == dungeongenerator::Cell::cellTypeEmpty) {
					continue;
				}

				// Build secondary command buffer
				VkCommandBufferInheritanceInfo inheritanceInfo = vks::initializers::commandBufferInheritanceInfo();
				inheritanceInfo.renderPass = deferredPass.renderPass;
				inheritanceInfo.framebuffer = deferredPass.frameBuffer;

				VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY, 1);
				vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cell->commandBuffer);

				VkCommandBufferBeginInfo commandBufferBeginInfo = vks::initializers::commandBufferBeginInfo();
				commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
				commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

				vkBeginCommandBuffer(cell->commandBuffer, &commandBufferBeginInfo);

				VkViewport viewport = vks::initializers::viewport((float)deferredPass.width, (float)deferredPass.height, 0.0f, 1.0f);
				vkCmdSetViewport(cell->commandBuffer, 0, 1, &viewport);

				VkRect2D scissor = vks::initializers::rect2D(deferredPass.width, deferredPass.height, 0, 0);
				vkCmdSetScissor(cell->commandBuffer, 0, 1, &scissor);

				vkCmdBindPipeline(cell->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.offscreen);

				VkDeviceSize offsets[1] = { 0 };

				vkCmdBindDescriptorSets(cell->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.offscreen, 0, 1, &descriptorSets.model, 0, NULL);

				vkCmdBindVertexBuffers(cell->commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
				vkCmdBindIndexBuffer(cell->commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

				vkCmdPushConstants(cell->commandBuffer, pipelineLayouts.offscreen, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);

				if (cell->type != dungeongenerator::Cell::cellTypeEmpty) {
					vkCmdDrawIndexed(cell->commandBuffer, 6, 1, INDEX_OFFSET_FLOOR, 0, 0);
					if (cell->walls[cell->dirNorth]) {
						vkCmdDrawIndexed(cell->commandBuffer, 6, 1, INDEX_OFFSET_WALL_NORTH, 0, 0);
					}
					if (cell->walls[cell->dirSouth]) {
						vkCmdDrawIndexed(cell->commandBuffer, 6, 1, INDEX_OFFSET_WALL_SOUTH, 0, 0);
					}
					if (cell->walls[cell->dirEast]) {
						vkCmdDrawIndexed(cell->commandBuffer, 6, 1, INDEX_OFFSET_WALL_EAST, 0, 0);
					}
					if (cell->walls[cell->dirWest]) {
						vkCmdDrawIndexed(cell->commandBuffer, 6, 1, INDEX_OFFSET_WALL_WEST, 0, 0);
					}
					if (!topdown) {
						vkCmdDrawIndexed(cell->commandBuffer, 6, 1, INDEX_OFFSET_CEILING, 0, 0);
					}
				}

				vkEndCommandBuffer(cell->commandBuffer);
			}
		}
	}

	/*
		Build command buffer for rendering the scene to the offscreen frame buffer attachments
	*/
	void buildDeferredCommandBuffer()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		// Clear values for all attachments written in the fragment sahder
		std::array<VkClearValue, 4> clearValues;
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[3].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = deferredPass.renderPass;
		renderPassBeginInfo.framebuffer = deferredPass.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width = deferredPass.width;
		renderPassBeginInfo.renderArea.extent.height = deferredPass.height;
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();

		VK_CHECK_RESULT(vkBeginCommandBuffer(deferredPass.commandBuffer, &cmdBufInfo));

		vkCmdBeginRenderPass(deferredPass.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		// Render dungeon cells
		cellsVisible = 0;

		frustum.update(camera.matrices.perspective * camera.matrices.view);

		std::vector<VkCommandBuffer> commandBuffers;
		#pragma omp parallel for
		for (int32_t x = 0; x < dungeon->width; x++) {
			for (int32_t y = 0; y < dungeon->height; y++) {
				if ((dungeon->cells[x][y]->type != dungeongenerator::Cell::cellTypeEmpty)) {
					glm::vec3 pos = glm::vec3(x - dungeon->width * 0.5f, 0.0, y - dungeon->height * 0.5f);
					glm::vec3 cpos = camera.position * glm::vec3(-1.0f, 0.0f, -1.0f);
					if (std::abs(glm::length(pos - cpos)) > maxDrawDistance) {
						continue;
					}
					dungeongenerator::Cell *cell = dungeon->cells[x][y];
					uint32_t frustumCheck = frustum.checkBox(pos, glm::vec3(0.5f, 2.5f, 0.5f));
					if (!(frustumCheck & 1)) {
						continue;
					}

					#pragma omp critical
					{
						if (cell->commandBuffer != VK_NULL_HANDLE) {
							commandBuffers.push_back(cell->commandBuffer);
							cellsVisible++;
						}
					}
				}
			}
		}

		if (commandBuffers.size() > 0) {
			vkCmdExecuteCommands(deferredPass.commandBuffer, commandBuffers.size(), commandBuffers.data());
		}

		vkCmdEndRenderPass(deferredPass.commandBuffer);

		VK_CHECK_RESULT(vkEndCommandBuffer(deferredPass.commandBuffer));
	}

	void loadAssets()
	{
		textureSets.default.load("default", vulkanDevice, queue);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.composition, 0, 1, &descriptorSet, 0, NULL);

			// Final composition as full screen quad
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.composition);
			vkCmdDraw(drawCmdBuffers[i], 4, 1, 0, 0);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 3);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		// Scene composition
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			// Binding 1 : Position texture target / Scene colormap
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			// Binding 2 : Normals texture target
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			// Binding 3 : Albedo texture target
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
			// Binding 4 : Fragment shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vks::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.composition));

		// Offscreen (scene) rendering pipeline layout
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::vec3), 0),
		};
		pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pPipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.offscreen));
	}

	void setupDescriptorSets()
	{
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;

		// Textured quad descriptor set
		VkDescriptorSetAllocateInfo allocInfo =
			vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		/*
			Scene composition
		*/

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

		// Image descriptors for the offscreen color attachments
		VkDescriptorImageInfo texDescriptorPosition =
			vks::initializers::descriptorImageInfo(deferredPass.sampler, deferredPass.position.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		VkDescriptorImageInfo texDescriptorNormal =
			vks::initializers::descriptorImageInfo(deferredPass.sampler, deferredPass.normal.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		VkDescriptorImageInfo texDescriptorAlbedo =
			vks::initializers::descriptorImageInfo(deferredPass.sampler, deferredPass.albedo.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		writeDescriptorSets = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.vsFullScreen.descriptor),
			// Binding 1 : Position texture target
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorPosition),
			// Binding 2 : Normals texture target
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
			// Binding 3 : Albedo texture target
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorAlbedo),
			// Binding 4 : Fragment shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &uniformBuffers.fsLights.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		/*
			Scene rendering
		*/
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
			// TODO: Number of combined image samplers from no. of loaded texture presets
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.model));
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.model, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.vsOffscreen.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.model, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textureSets.default.color.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		
		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
			vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vks::initializers::pipelineCreateInfo(pipelineLayouts.composition, renderPass, 0);

		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		// Final fullscreen composition pass pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Empty vertex input state, full screen quad is generated in VS
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCreateInfo.pVertexInputState = &emptyInputState;
		pipelineCreateInfo.layout = pipelineLayouts.composition;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.composition));

		// Deferred offscreen rendering pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/deferred.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/deferred.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Vertex bindings an attributes
		// Binding description
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
		};
		// Attribute descriptions
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Normal
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 6),	// UV
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();
		pipelineCreateInfo.pVertexInputState = &vertexInputState;
		pipelineCreateInfo.renderPass = deferredPass.renderPass;
		pipelineCreateInfo.layout = pipelineLayouts.offscreen;
		std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
		};
		colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
		colorBlendState.pAttachments = blendAttachmentStates.data();
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.offscreen));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Fullscreen vertex shader
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.vsFullScreen,
			sizeof(uboVS)));

		// Deferred vertex shader
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.vsOffscreen,
			sizeof(uboOffscreenVS)));

		// Deferred fragment shader
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.fsLights,
			sizeof(uboFragmentLights)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.vsFullScreen.map());
		VK_CHECK_RESULT(uniformBuffers.vsOffscreen.map());
		VK_CHECK_RESULT(uniformBuffers.fsLights.map());

		// Update
		updateUniformBuffersScreen();
		updateUniformBufferDeferredMatrices();
		updateUniformBufferDeferredLights();
	}

	void updateUniformBuffersScreen()
	{
		uboVS.projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
		uboVS.model = glm::mat4(1.0f);
		memcpy(uniformBuffers.vsFullScreen.mapped, &uboVS, sizeof(uboVS));
	}

	void updateUniformBufferDeferredMatrices()
	{
		uboOffscreenVS.projection = camera.matrices.perspective;
		uboOffscreenVS.view = camera.matrices.view;
		uboOffscreenVS.model = glm::mat4(1.0f);

		memcpy(uniformBuffers.vsOffscreen.mapped, &uboOffscreenVS, sizeof(uboOffscreenVS));
	}

	// Update fragment shader light position uniform block
	void updateUniformBufferDeferredLights()
	{
		// White
		uboFragmentLights.lights[0].position = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
		uboFragmentLights.lights[0].color = glm::vec3(1.5f);
		uboFragmentLights.lights[0].radius = 15.0f * 0.25f;
		// Red
		uboFragmentLights.lights[1].position = glm::vec4(-2.0f, 0.0f, 0.0f, 0.0f);
		uboFragmentLights.lights[1].color = glm::vec3(1.0f, 0.0f, 0.0f);
		uboFragmentLights.lights[1].radius = 15.0f;
		// Blue
		uboFragmentLights.lights[2].position = glm::vec4(2.0f, 1.0f, 0.0f, 0.0f);
		uboFragmentLights.lights[2].color = glm::vec3(0.0f, 0.0f, 2.5f);
		uboFragmentLights.lights[2].radius = 5.0f;
		// Yellow
		uboFragmentLights.lights[3].position = glm::vec4(0.0f, 0.9f, 0.5f, 0.0f);
		uboFragmentLights.lights[3].color = glm::vec3(1.0f, 1.0f, 0.0f);
		uboFragmentLights.lights[3].radius = 2.0f;
		// Green
		uboFragmentLights.lights[4].position = glm::vec4(0.0f, 0.5f, 0.0f, 0.0f);
		uboFragmentLights.lights[4].color = glm::vec3(0.0f, 1.0f, 0.2f);
		uboFragmentLights.lights[4].radius = 5.0f;
		// Yellow
		uboFragmentLights.lights[5].position = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
		uboFragmentLights.lights[5].color = glm::vec3(1.0f, 0.7f, 0.3f);
		uboFragmentLights.lights[5].color = glm::vec3(1.0f);
		uboFragmentLights.lights[5].radius = 10.0f;
		uboFragmentLights.lights[5].position = glm::vec4(camera.position, 0.0f) * -1.0f;

		uboFragmentLights.lights[0].position.x = sin(glm::radians(360.0f * timer)) * 5.0f;
		uboFragmentLights.lights[0].position.z = cos(glm::radians(360.0f * timer)) * 5.0f;

		uboFragmentLights.lights[1].position.x = -4.0f + sin(glm::radians(360.0f * timer) + 45.0f) * 2.0f;
		uboFragmentLights.lights[1].position.z = 0.0f + cos(glm::radians(360.0f * timer) + 45.0f) * 2.0f;

		uboFragmentLights.lights[2].position.x = 4.0f + sin(glm::radians(360.0f * timer)) * 2.0f;
		uboFragmentLights.lights[2].position.z = 0.0f + cos(glm::radians(360.0f * timer)) * 2.0f;

		uboFragmentLights.lights[4].position.x = 0.0f + sin(glm::radians(360.0f * timer + 90.0f)) * 5.0f;
		uboFragmentLights.lights[4].position.z = 0.0f - cos(glm::radians(360.0f * timer + 45.0f)) * 5.0f;

		//uboFragmentLights.lights[5].position.x = 0.0f + sin(glm::radians(-360.0f * timer + 135.0f)) * 10.0f;
		//uboFragmentLights.lights[5].position.z = 0.0f - cos(glm::radians(-360.0f * timer - 45.0f)) * 10.0f;

		// Current view position
		uboFragmentLights.viewPos = glm::vec4(camera.position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

		memcpy(uniformBuffers.fsLights.mapped, &uboFragmentLights, sizeof(uboFragmentLights));
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		// Offscreen rendering

		// Wait for swap chain presentation to finish
		submitInfo.pWaitSemaphores = &semaphores.presentComplete;
		// Signal ready with offscreen semaphore
		submitInfo.pSignalSemaphores = &deferredPass.semaphore;

		// Submit work
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &deferredPass.commandBuffer;
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		// Scene rendering

		// Wait for offscreen semaphore
		submitInfo.pWaitSemaphores = &deferredPass.semaphore;
		// Signal ready with render complete semaphpre
		submitInfo.pSignalSemaphores = &semaphores.renderComplete;

		// Submit work
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		preparedeferredPassfer();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSets();
		buildVertexBuffers();

		generateCellCommandBuffers();

		buildCommandBuffers();

		deferredPass.commandBuffer = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &deferredPass.semaphore));

		buildDeferredCommandBuffer();

		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
		updateUniformBufferDeferredLights();
	}

	virtual void viewChanged()
	{
		buildDeferredCommandBuffer();
		updateUniformBufferDeferredMatrices();
	}

	virtual void keyPressed(uint32_t keyCode)
	{
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
		textOverlay->addText(std::to_string(cellsVisible), 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
	}
};

VULKAN_EXAMPLE_MAIN()
