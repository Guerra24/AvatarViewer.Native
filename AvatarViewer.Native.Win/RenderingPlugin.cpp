#include "pch.h"

#include <wrl.h>
#include <assert.h>
#include <format>

#include "RenderingPlugin.h"
#include "D3DPriority.h"

using namespace Microsoft::WRL;

void SetupD3DPriority(_D3DKMT_SCHEDULINGPRIORITYCLASS priority) {
	UNITY_LOG(s_UnityLog, "Setup GPU Process priority");
	HMODULE gdi32 = GetModuleHandleA("GDI32");
	if (gdi32) {
		/*auto check_hags = [&](const LUID& adapter) -> bool {
			auto d3dkmt_open_adapter = (PD3DKMTOpenAdapterFromLuid)GetProcAddress(gdi32, "D3DKMTOpenAdapterFromLuid");
			auto d3dkmt_query_adapter_info = (PD3DKMTQueryAdapterInfo)GetProcAddress(gdi32, "D3DKMTQueryAdapterInfo");
			auto d3dkmt_close_adapter = (PD3DKMTCloseAdapter)GetProcAddress(gdi32, "D3DKMTCloseAdapter");
			if (!d3dkmt_open_adapter || !d3dkmt_query_adapter_info || !d3dkmt_close_adapter) {
				//BOOST_LOG(error) << "Couldn't load d3dkmt functions from gdi32.dll to determine GPU HAGS status";
			}

			D3DKMT_OPENADAPTERFROMLUID d3dkmt_adapter = { adapter };
			if (FAILED(d3dkmt_open_adapter(&d3dkmt_adapter))) {
				//BOOST_LOG(error) << "D3DKMTOpenAdapterFromLuid() failed while trying to determine GPU HAGS status";
			}

			bool result;

			D3DKMT_WDDM_2_7_CAPS d3dkmt_adapter_caps = {};
			D3DKMT_QUERYADAPTERINFO d3dkmt_adapter_info = {};
			d3dkmt_adapter_info.hAdapter = d3dkmt_adapter.hAdapter;
			d3dkmt_adapter_info.Type = KMTQAITYPE_WDDM_2_7_CAPS;
			d3dkmt_adapter_info.pPrivateDriverData = &d3dkmt_adapter_caps;
			d3dkmt_adapter_info.PrivateDriverDataSize = sizeof(d3dkmt_adapter_caps);

			if (SUCCEEDED(d3dkmt_query_adapter_info(&d3dkmt_adapter_info))) {
				result = d3dkmt_adapter_caps.HwSchEnabled;
			}
			else {
				//BOOST_LOG(warning) << "D3DKMTQueryAdapterInfo() failed while trying to determine GPU HAGS status";
				result = false;
			}

			D3DKMT_CLOSEADAPTER d3dkmt_close_adapter_wrap = { d3dkmt_adapter.hAdapter };
			if (FAILED(d3dkmt_close_adapter(&d3dkmt_close_adapter_wrap))) {
				//BOOST_LOG(error) << "D3DKMTCloseAdapter() failed while trying to determine GPU HAGS status";
			}

			return result;
		};*/

		auto d3dkmt_set_process_priority = (PD3DKMTSetProcessSchedulingPriorityClass)GetProcAddress(gdi32, "D3DKMTSetProcessSchedulingPriorityClass");
		if (d3dkmt_set_process_priority) {
			//bool hags_enabled = check_hags(adapter_desc.AdapterLuid);
			//if (adapter_desc.VendorId == 0x10DE) {
				// As of 2023.07, NVIDIA driver has unfixed bug(s) where "realtime" can cause unrecoverable encoding freeze or outright driver crash
				// This issue happens more frequently with HAGS, in DX12 games or when VRAM is filled close to max capacity
				// Track OBS to see if they find better workaround or NVIDIA fixes it on their end, they seem to be in communication
				//if (hags_enabled && !config::video.nv_realtime_hags) priority = D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH;
			//}
			//BOOST_LOG(info) << "Active GPU has HAGS " << (hags_enabled ? "enabled" : "disabled");
			//BOOST_LOG(info) << "Using " << (priority == D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH ? "high" : "realtime") << " GPU priority";
			UNITY_LOG(s_UnityLog, std::format("Using {0} GPU Process priority", ToString(priority)).c_str());
			if (FAILED(d3dkmt_set_process_priority(GetCurrentProcess(), priority))) {
				UNITY_LOG_WARNING(s_UnityLog, "Failed to adjust GPU process priority. Please run application as administrator for optimal performance.");
			}
		}
		else
		{
			UNITY_LOG_WARNING(s_UnityLog, "Couldn't load D3DKMTSetProcessSchedulingPriorityClass function from gdi32.dll to adjust GPU priority");
		}
	}
}

template <typename T> static void IncreaseGPUPriority(T* device, int priority) {
	UNITY_LOG(s_UnityLog, "Setup GPU thread priority");
	ComPtr<IDXGIDevice> dxgiDevice;
	auto hr = device->QueryInterface(__uuidof(dxgiDevice), &dxgiDevice);
	assert(SUCCEEDED(hr));

	UNITY_LOG(s_UnityLog, std::format("Using {0} GPU Thread priority", priority).c_str());
	dxgiDevice->SetGPUThreadPriority(priority);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ChangePriority(_D3DKMT_SCHEDULINGPRIORITYCLASS processPriority, int gpuThreadPriority) {
	SetupD3DPriority(processPriority);
	if (s_Graphics->GetRenderer() == kUnityGfxRendererD3D11)
	{
		auto unityD3D11 = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
		IncreaseGPUPriority<ID3D11Device>(unityD3D11->GetDevice(), gpuThreadPriority);
	}
	else if (s_Graphics->GetRenderer() == kUnityGfxRendererD3D12)
	{
		auto unityD3D12 = s_UnityInterfaces->Get<IUnityGraphicsD3D12>();
		IncreaseGPUPriority<ID3D12Device>(unityD3D12->GetDevice(), gpuThreadPriority);
	}
}

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_UnityLog = unityInterfaces->Get<IUnityLog>();
	s_Graphics = unityInterfaces->Get<IUnityGraphics>();
	UNITY_LOG(s_UnityLog, "GPU Priority plugin registered...");

	//s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	//s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

/*
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	switch (eventType)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		UNITY_LOG(s_UnityLog, "Starting plugin...");
		if (s_Graphics->GetRenderer() == kUnityGfxRendererD3D11)
		{
			auto unityD3D11 = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
			auto d3d11Device = unityD3D11->GetDevice();
			SetupD3DPriority();
			IncreaseGPUPriority<ID3D11Device>(d3d11Device);
		}
		else if (s_Graphics->GetRenderer() == kUnityGfxRendererD3D12)
		{
			auto unityD3D12 = s_UnityInterfaces->Get<IUnityGraphicsD3D12>();
			auto d3d12Device = unityD3D12->GetDevice();
			SetupD3DPriority();
			IncreaseGPUPriority<ID3D12Device>(d3d12Device);
		}
		break;
	}
	case kUnityGfxDeviceEventShutdown:
	{
		break;
	}
	case kUnityGfxDeviceEventBeforeReset:
	{
		break;
	}
	case kUnityGfxDeviceEventAfterReset:
	{
		break;
	}
	};
}
*/