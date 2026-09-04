#pragma once

#include "PluginAPI.h"

// Legacy, pre-6.4.9 NVSE version
// Used only for backwards compatibility
// Legacy extra datas are kept separate, and require their specific functions to add and retrieve
class [[deprecated]] LegacyPluginFormExtraData
{
public:
	NiFixedString	name;
	UInt32			refCount = 0;

	LegacyPluginFormExtraData(const NiFixedString& aName) : name(aName), refCount(0) {}
	virtual ~LegacyPluginFormExtraData() {};
	virtual void DeleteThis() {
		this->~LegacyPluginFormExtraData();
		FormHeap_Free(this);
	};

	void IncRefCount() noexcept {
		InterlockedIncrement(&refCount);
	}

	void DecRefCount() noexcept {
		if (InterlockedDecrement(&refCount) == 0) {
			DeleteThis();
		}
	}

	// Retrieves extra data from a form by name (case sensitive).
	[[nodiscard]] 
	static inline LegacyPluginFormExtraData* Get(NVSEDataInterface* dataApi, const TESForm* form, const char* name) noexcept
	{
		static auto* get = (LegacyPluginFormExtraData*(*)(const TESForm*, const char*)) dataApi->GetFunc(NVSEDataInterface::kNVSEData_LegacyFormExtraDataGet);
		return get(form, name);
	}

	// Adds extra data to a form.
	// Returns true if the extra data was added successfully, false if it already exists, or arguments are null.
	static inline bool Add(NVSEDataInterface* dataApi, TESForm* form, LegacyPluginFormExtraData* extraData) noexcept
	{
		static auto* add = (bool(*)(TESForm*, LegacyPluginFormExtraData*)) dataApi->GetFunc(NVSEDataInterface::kNVSEData_LegacyFormExtraDataAdd);
		return add(form, extraData);
	}

	// Removes extra data from a form by name (case sensitive).
	static inline void Remove(NVSEDataInterface* dataApi, TESForm* form, const char* name) noexcept
	{
		static auto* remove = (void (*)(TESForm*, const char*)) dataApi->GetFunc(NVSEDataInterface::kNVSEData_LegacyFormExtraDataRemoveByName);
		remove(form, name);
	}

	// Removes extra data from a form by pointer to the data.
	static inline void Remove(NVSEDataInterface* dataApi, TESForm* form, LegacyPluginFormExtraData* extraData) noexcept
	{
		static auto* remove = (void (*)(TESForm*, LegacyPluginFormExtraData*)) dataApi->GetFunc(NVSEDataInterface::kNVSEData_LegacyFormExtraDataRemoveByPtr);
		remove(form, extraData);
	}

	// Retrieves all extra data from a form.
	// First query the data count with an empty outData pointer, then call again with an appropriately sized outData array.
	static inline UInt32 GetAllExtraData(NVSEDataInterface* dataApi, const TESForm* form, LegacyPluginFormExtraData** outData) noexcept {
		static auto* getAll = (UInt32(*)(const TESForm*, LegacyPluginFormExtraData**)) dataApi->GetFunc(NVSEDataInterface::kNVSEData_LegacyFormExtraDataGetAll);
		return getAll(form, outData);
	}
};