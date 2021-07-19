#ifndef __VULKAN_RENDERER_H__
#define	__VULKAN_RENDERER_H__

#include <vector>
#include <set>
#include <array>
#include <optional>

#include "Renderer.h"

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	std::optional<uint32_t> computeFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
	}
};

class Texture;
class Material;
class PointLight;
class VulkanRenderer : public Renderer
{
public:
	VulkanRenderer(GLFWwindow* win);
	virtual ~VulkanRenderer();

	virtual void RenderBegin();
	virtual void RenderEnd();
	virtual void Flush();
	virtual void WaitIdle();

	virtual GeoData* CreateGeoData();
	virtual void Draw(GeoData* geoData, std::vector<Material*>& mats);

	virtual void UpdateCameraMatrix();
	virtual void UpdateTransformMatrix(TransformEntity* transform);

	virtual void AddLight(PointLight* light);
	virtual void ClearLight();

	virtual int GetFrameBufferCount() { return 2; }
	virtual int GetFrameIndex() { return active_command_buffer_idx; }

	virtual void OnSceneExit();

	inline VkCommandBuffer CurrentCommandBuffer() { return command_buffers[active_command_buffer_idx]; }

	void CreateVertexBuffer( void* vdata, uint32_t single, uint32_t length, VkBuffer& buffer, VkDeviceMemory& mem);
	void CreateIndexBuffer(void* idata, uint32_t single, uint32_t length, VkBuffer& buffer, VkDeviceMemory& mem);
	void CreateImageBuffer(void* imageData, uint32_t length, VkBuffer& buffer, VkDeviceMemory& mem);
	void CreateLocalStorageBuffer(void** data, uint32_t length, VkBuffer& buffer, VkDeviceMemory& mem);
	void CreateLocalStorageBufferWithData(void* data, uint32_t length, VkBuffer& buffer, VkDeviceMemory& mem, VkDescriptorBufferInfo& info);
	void CreateGraphicsStorageBuffer(void** data, uint32_t length, VkBuffer& buffer, VkDeviceMemory& mem);
	void CreateUniformBuffer(void** data, uint32_t length, VkBuffer& buffer, VkDeviceMemory& mem);
	void UnmapBufferMemory(VkDeviceMemory& mem);
	void CleanBuffer(VkBuffer& buffer, VkDeviceMemory& mem);

	void ClearLightBufferData();

	void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void CleanImage(VkImage& image, VkDeviceMemory& imageMem, VkImageView& imageView);
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	void SetMvpMatrix(glm::mat4x4& mvpMtx);
	void SetModelMatrix(glm::mat4x4& mtx);
	void SetViewMatrix(glm::mat4x4& mtx);
	void SetProjMatrix(glm::mat4x4& mtx);
	void SetProjViewMatrix(glm::mat4x4& mtx);
	void SetCamPos(glm::vec3& pos);
	void SetLightPos(glm::vec4& pos, int idx);
	void SetTexture(Texture* tex);
	void SetNormalTexture(Texture* tex);

	void AllocateDescriptorSets(VkDescriptorSet* descSets);
	void UpdateMaterial(Material* mat);
	void FreeDescriptorSets(VkDescriptorSet* descSets);

	void AllocateMeshletDescriptorSets(VkDescriptorSet* descSets);
	void UploadMeshlets(VkDescriptorBufferInfo* vertexBufInfo, VkDescriptorBufferInfo* meshletBufInfo, VkDescriptorBufferInfo* vertexIndicesBufInfo, VkDescriptorBufferInfo* primtiveIndicesBufInfo, VkDescriptorSet* descSets);
	void BindMeshlets(VkDescriptorSet* descSets);
	void FreeMeshletDescriptorSets(VkDescriptorSet* descSets);

	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	void CreateTextureSampler(VkSampler* sampler);
	void DestroyTextureSampler(VkSampler* sampler);

	void UpdateComputeDescriptorSet();

	bool IsMeshShading() { return isMeshShader; }
	void SetMeshShading(bool _isMeshShading) { isMeshShaderState = _isMeshShading; }

	bool IsClusteShading() { return isClusteShading; }
	void SetClusteShading(bool _isClusteShading) { isClusteShadingState = _isClusteShading; }

	bool IsISPC() { return isIspc; }
	void SetISPC(bool _isIspc) { isIspcState = _isIspc; }

	bool IsCpuClusteCull() { return isCpuClusteCull; }
	void SetCpuClusteCull(bool _isCpuClusteCull) { isCpuClusteCullState = _isCpuClusteCull; }

	double GetCpuCullTime() { return cpuCullTime; }
	double GetGpuCullTime() { return (double)gpuCullTime / timestampFrequency; }

	uint32_t GetMaxDrawMeshTaskCount();
	bool IsMeshShadingSupported() { return is_mesh_shading_supported; }

private:
	std::array<VkVertexInputBindingDescription, 1> GetBindingDescription();
	std::array<VkVertexInputAttributeDescription, 5> GetAttributeDescriptions();

	void CreateInstance();
	bool CheckValidationLayerSupport();

	void CreateSurface();

	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

	void CreateLogicDevice();

	void CreateSwapChain();
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	void CreateImageViews();

	void CreateRenderPass();

	void CreateGraphicsPipeline();
	VkShaderModule createShaderModule(const std::vector<char>& code);

	void InitializeClusteRendering();
	void CreateCompPipeline();

	void CreateDepthResources();
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat FindDepthFormat();
	bool HasStencilComponent(VkFormat format);

	void CreateFramebuffers();

	void CreateCommandPool();
	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	void CreateCommandBuffers();

	void CreateUniformBuffers();

	void CreateDescriptorSetsPool();

	void CreateMeshletDescriptorSetsPool();

	void CreateCompDescriptorSetsPool();
	void AllocateCompDescriptorSets(VkDescriptorSet* descSets);
	void FreeCompDescriptorSets(VkDescriptorSet* descSets);

	void CreateCompDescriptorSets();
	void ReleaseCompDescriptorSets();

	void CreateSemaphores();

	void SetScreenToViewData(ScreenToView* stv);

	void CleanUp();

private:
	VkInstance instance;
	VkPhysicalDevice physical_device;
	uint32_t max_draw_mesh_tasks_count;
	bool is_mesh_shading_supported;
	VkDevice device;
	VkQueue graphics_queue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swap_chain;
	std::vector<VkImage> swap_chain_images;
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	std::vector<VkImageView> swap_chain_image_views;
	VkShaderModule vert_shader_module;
	VkShaderModule frag_shader_module;
	VkShaderModule task_shader_module;
	VkShaderModule mesh_shader_module;
	VkRenderPass render_pass;
	VkDescriptorSetLayout desc_layout;
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout meshlet_desc_layout;
	VkDescriptorPool meshlet_desc_pool;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	VkPipeline mesh_pipeline;
	std::vector<VkFramebuffer> swap_chain_framebuffers;
	VkCommandPool command_pool;
	std::vector<VkCommandBuffer> command_buffers;
	VkSemaphore image_available_semaphore;
	VkSemaphore render_finished_semaphore;
	VkSemaphore compute_finished_semaphore;
	VkFence in_flight_fence;
	VkImage depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView depth_image_view;

	uint32_t active_command_buffer_idx;
	uint32_t last_command_buffer_idx;

	VkDescriptorImageInfo* image_info;
	VkDescriptorImageInfo* normal_image_info;

	/// uniform buffers
	VkBuffer transform_uniform_buffer;
	VkDeviceMemory transform_uniform_buffer_memory;
	VkDescriptorBufferInfo transform_uniform_buffer_info;
	void* transform_uniform_buffer_data;

	std::vector<VkBuffer> light_uniform_buffers;
	std::vector<VkDeviceMemory> light_uniform_buffer_memorys;
	std::vector<VkDescriptorBufferInfo> light_uniform_buffer_infos;
	std::vector<void*> light_uniform_buffer_datas;

	/// cluste calculate
	unsigned int tile_size_x;	/// ss width height
	glm::uvec3 group_num;
	VkDescriptorPool comp_desc_pool;
	VkDescriptorSetLayout comp_desc_layout;
	VkPipelineLayout comp_pipeline_layout;
	VkPipeline comp_pipelines[2];
	VkDescriptorSet comp_desc_set[3];
	VkCommandBuffer comp_command_buffers[2*3];
	VkQueue comp_queue;
	VkCommandPool comp_command_pool;
	VkFence comp_wait_fence;
	VkShaderModule comp_cluste_shader_module;
	VkShaderModule cluste_cull_shader_module;
	VkQueryPool query_pool[2*3];

	/// tile aabb
	VkBuffer tile_aabbs_buffer;
	VkDeviceMemory tile_aabbs_buffer_memory;
	void* tile_aabbs_buffer_data;
	VkDescriptorBufferInfo tile_aabbs_buffer_info;

	/// screen to view
	VkBuffer screen_to_view_buffer;
	VkDeviceMemory screen_to_view_buffer_memory;
	void* screen_to_view_buffer_data;
	VkDescriptorBufferInfo screen_to_view_buffer_info;

	/// light datas
	VkBuffer light_datas_buffer;
	VkDeviceMemory light_datas_buffer_memory;
	void* light_datas_buffer_data;
	VkDescriptorBufferInfo light_datas_buffer_info;

	/// light indexes
	VkBuffer local_light_indexes_buffer;
	VkDeviceMemory local_light_indexes_buffer_memory;
	VkBuffer gpu_light_indexes_buffer;
	VkDeviceMemory gpu_light_indexes_buffer_memory;
	void* light_indexes_buffer_data;
	VkDescriptorBufferInfo local_light_indexes_buffer_info;
	VkDescriptorBufferInfo gpu_light_indexes_buffer_info;

	/// light grids
	VkBuffer local_light_grids_buffer;
	VkDeviceMemory local_light_grids_buffer_memory;
	VkBuffer gpu_light_grids_buffer;
	VkDeviceMemory gpu_light_grids_buffer_memory;
	void* light_grids_buffer_data;
	VkDescriptorBufferInfo local_light_grids_buffer_info;
	VkDescriptorBufferInfo gpu_light_grids_buffer_info;

	/// index count
	VkBuffer index_count_buffer;
	VkDeviceMemory index_count_buffer_memory;
	void* index_count_buffer_data;
	VkDescriptorBufferInfo index_count_buffer_info;

	bool isClusteShading;
	bool isClusteShadingState;
	bool isIspc;
	bool isIspcState;
	bool isCpuClusteCull;
	bool isCpuClusteCullState;
	bool isMeshShader;
	bool isMeshShaderState;

	bool isTaskShaderInit;

	double cpuCullTime;
	uint64_t gpuCullTime;
	bool compSupportTimeStamp;
	float timestampPeriod;
	double timestampFrequency;
};

#endif // !__VULKAN_RENDERER_H__