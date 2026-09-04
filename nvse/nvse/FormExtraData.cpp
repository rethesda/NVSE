#include "FormExtraData.h"
#include "SafeWrite.h"
#include <shared_mutex>
#include <ranges>

namespace 
{
	// We're abusing the fact that maps are templates and singletons, and making methods and members static
	// This allows us to use __fastcall fully (as both arguments are passed through registers), nothing is wasted on storing "this" in ecx
	// The compiler inlines the singletons either way, so ecx goes to waste with __thiscall methods

	template<class T>
	class FormExtraDataMap {
	public:
		using _Array = std::vector<NiPointer<T>>;
		using _Map	 = std::unordered_map<const TESForm*, _Array>;

		static inline std::shared_mutex mutex;
		static inline _Map				formMap;

		static bool __fastcall AddData(TESForm* form, T* formExtraData) noexcept {
			const NiFixedString name = formExtraData->GetName();
			if (!name) [[unlikely]]
				return false;

			std::unique_lock lock(mutex);

			_Array& array = formMap[form];

			if (std::ranges::any_of(array, [&](const NiPointer<T>& data) { return data && data->GetName() == name; })) [[unlikely]] {
				return false; // Already exists
			}

			array.emplace_back(formExtraData);

			return true;
		}
	
		static bool __fastcall RemoveByName(TESForm* form, const char* name) noexcept {
			NiPointer<T> storedData;
			{
				std::unique_lock lock(mutex);

				auto iter = formMap.find(form);
				if (iter != formMap.end()) [[likely]] {
					_Array& array = iter->second;

					std::erase_if(array, [&](const NiPointer<T>& data) {
						if (data && data->GetName() == name) {
							storedData = data;
							return true;
						}
						return false;
						}
					);

					if (array.empty())
						formMap.erase(iter);
				}
			}
			if (storedData)
				storedData->OnRemoval(form, FormExtraData::RemovalReason::kManualRequest);

			return storedData;
		}

		static bool __fastcall RemoveByPtr(TESForm* form, T* formExtraData) noexcept {
			NiPointer<T> storedData;
			{
				std::unique_lock lock(mutex);

				auto iter = formMap.find(form);
				if (iter != formMap.end()) [[likely]] {
					_Array& array = iter->second;

					std::erase_if(array, [&](const NiPointer<T>& data) {
						if (data == formExtraData) {
							storedData = data;
							return true;
						}
						return false;
						}
					);

					if (array.empty())
						formMap.erase(iter);
				}
			}
			if (storedData)
				storedData->OnRemoval(form, FormExtraData::RemovalReason::kManualRequest);

			return storedData;
		}

		static void __fastcall RemoveForForm(TESForm* form, FormExtraData::RemovalReason reason) noexcept {
			_Array storedData;
			{
				std::unique_lock lock(mutex);

				auto iter = formMap.find(form);
				if (iter != formMap.end()) {
					storedData = std::move(iter->second);
					formMap.erase(iter);
				}
			}
			for (auto& data : storedData) {
				data->OnRemoval(form, reason);
			}
		}
	
		static T* __fastcall Get(const TESForm* form, const char* name) noexcept {
			std::shared_lock lock(mutex);

			const auto iter = formMap.find(form);
			if (iter != formMap.end()) [[likely]] {
				for (const auto& data : iter->second) {
					if (data && data->GetName() == name)
						return data;
				}
			}
			return nullptr;
		}

		template<class arrayItem>
		static UInt32 __fastcall GetAll(const TESForm* form, arrayItem* outData) noexcept {
			std::shared_lock lock(mutex);

			UInt32 count = 0;
			const auto iter = formMap.find(form);
			if (iter != formMap.end()) [[likely]] {
				const _Array& array = iter->second;
				count = static_cast<UInt32>(array.size());

				if (count && outData) {
					for (UInt32 i = 0; i < count; ++i) {
						outData[i] = array[i];
					}
				}
					
			}
			return count;
		}
	};

	using ExtraDataFormMap			= FormExtraDataMap<FormExtraData>;
	using LegacyExtraDataFormMap	= FormExtraDataMap<LegacyFormExtraData>;

	static constexpr inline ExtraDataFormMap g_formExtraDataMap;
	static constexpr inline LegacyExtraDataFormMap g_legacyFormExtraDataMap;
}

bool __fastcall FormExtraDataManager::Add(TESForm* form, FormExtraData* formExtraData, bool legacyMode) noexcept
{
	if (!form || !formExtraData) [[unlikely]]
		return false;

	if (legacyMode) [[unlikely]] {
		return g_legacyFormExtraDataMap.AddData(form, reinterpret_cast<LegacyFormExtraData*>(formExtraData));
	}
	else [[likely]] {
		if (formExtraData->GetVersion() > FormExtraData::kVersion) [[unlikely]] {
#ifdef _DEBUG
			_DMESSAGE("Tried to add FormExtraData with a version newer than supported! (Got %i, max supported is %i)", formExtraData->GetVersion(), FormExtraData::kVersion);
			DebugBreak();
#endif
			return false;
		}
		return g_formExtraDataMap.AddData(form, formExtraData);
	}
}

bool __fastcall FormExtraDataManager::RemoveByName(TESForm* form, const char* name, bool legacyMode) noexcept
{
	if (!form || !name) [[unlikely]]
		return false;

	if (legacyMode) [[unlikely]] {
		return g_legacyFormExtraDataMap.RemoveByName(form, name);
	}
	else [[likely]] {
		return g_formExtraDataMap.RemoveByName(form, name);
	}
}

bool __fastcall FormExtraDataManager::RemoveByPtr(TESForm* form, FormExtraData* formExtraData, bool legacyMode) noexcept
{
	if (!form || !formExtraData) [[unlikely]]
		return false;

	if (legacyMode) [[unlikely]] {
		return g_legacyFormExtraDataMap.RemoveByPtr(form, reinterpret_cast<LegacyFormExtraData*>(formExtraData));
	}
	else [[likely]] {
		return g_formExtraDataMap.RemoveByPtr(form, formExtraData);
	}
}

FormExtraData* __fastcall FormExtraDataManager::Get(const TESForm* form, const char* name, bool legacyMode) noexcept
{
	if (!form || !name) [[unlikely]]
		return nullptr;

	if (legacyMode) [[unlikely]] {
		return reinterpret_cast<FormExtraData*>(g_legacyFormExtraDataMap.Get(form, name));
	}
	else [[likely]] {
		return g_formExtraDataMap.Get(form, name);
	}
}

UInt32 __fastcall FormExtraDataManager::GetAll(const TESForm* form, NiPointer<FormExtraData>* outData) noexcept
{
	if (!form) [[unlikely]]
		return 0;

	return g_formExtraDataMap.GetAll<NiPointer<FormExtraData>>(form, outData);
}

UInt32 __fastcall FormExtraDataManager::LegacyGetAll(const TESForm* form, LegacyFormExtraData** outData) noexcept
{
	if (!form) [[unlikely]]
		return 0;

	return g_legacyFormExtraDataMap.GetAll<LegacyFormExtraData*>(form, outData);
}

namespace Hooks {

	namespace {
		__declspec(noinline) void __fastcall RemoveForm(TESForm* form, FormExtraData::RemovalReason reason) noexcept {
			g_formExtraDataMap.RemoveForForm(form, reason);
			{
				[[unlikely]]
				g_legacyFormExtraDataMap.RemoveForForm(form, reason);
			}
		}
	}

	template <UInt32 address, FormExtraData::RemovalReason reason>
	class RemoveFromAllFormsMapHook {
		static inline UInt32 replacedAddress = 0;

		static bool __fastcall Hook(TESForm* form) noexcept {
			RemoveForm(form, reason);
			return ThisStdCall<bool>(replacedAddress, form);
		}

	public:
		RemoveFromAllFormsMapHook() noexcept {
			WriteRelCall(address, &RemoveFromAllFormsMapHook::Hook, &replacedAddress);
		}
	};

	void InitHooks() 
	{
#if RUNTIME
		RemoveFromAllFormsMapHook<0x483669, FormExtraData::RemovalReason::kFormDeletion>();
		RemoveFromAllFormsMapHook<0x8680A4, FormExtraData::RemovalReason::kTrashedReference>();
#else
		RemoveFromAllFormsMapHook<0x4FD0C7, FormExtraData::RemovalReason::kFormDeletion>();
		// GECK has no garbage collector
#endif
	}

}



void FormExtraDataManager::WriteHooks() noexcept
{
	Hooks::InitHooks();
}
