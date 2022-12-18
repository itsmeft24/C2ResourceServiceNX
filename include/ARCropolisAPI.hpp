#pragma once

#include <optional>
#include <functional>
#include <span>
#include <crc32.h>
#include <ARCTypes.hpp>

namespace ARCropolis {

	enum class Event : uint32_t
	{
		ARCFilesystemMounted,
		ModFilesystemMounted
	};

	struct APIVersion {
		uint32_t Major;
		uint32_t Minor;
	};

	typedef bool (*CallbackFunction)(uint64_t, uint8_t*, uint64_t, uint64_t*);
	typedef bool (*StreamCallbackFunction)(uint64_t, uint8_t*, uint64_t*);
	typedef bool (*ExtensionCallbackFunction)(uint64_t, uint8_t*, uint64_t, uint64_t*);
	typedef bool (*EventCallbackFunction)(Event);

	extern "C" {
		void arcrop_register_callback(uint64_t, uint64_t, CallbackFunction);
		void arcrop_register_callback_with_path(uint64_t, uint64_t, StreamCallbackFunction);
		bool arcrop_load_file(uint64_t, uint8_t*, uint64_t, uint64_t*);
		static APIVersion* arcrop_api_version();
		void arcrop_require_api_version(uint32_t, uint32_t);
		void arcrop_register_extension_callback(uint64_t, ExtensionCallbackFunction);
		bool arcrop_get_decompressed_size(uint64_t, uint64_t*);
		void arcrop_register_event_callback(Event, EventCallbackFunction);
		bool arcrop_is_file_loaded(uint64_t);
	}

	inline void register_callback(C2ResourceServiceNX::Hash40 hash, uint64_t max_size, CallbackFunction clbk) {
		arcrop_register_callback(hash.as_u64(), max_size, clbk);
	}

	inline void register_stream_callback(C2ResourceServiceNX::Hash40 hash, uint64_t max_size, StreamCallbackFunction clbk) {
		arcrop_register_callback_with_path(hash.as_u64(), max_size, clbk);
	}

	inline bool load_file(C2ResourceServiceNX::Hash40 hash, uint8_t* buffer, uint64_t len, uint64_t* out_size) {
		return arcrop_load_file(hash.as_u64(), buffer, len, out_size);
	}

	inline APIVersion* get_api_version() {
		return arcrop_api_version();
	}

	inline void require_api_version(const APIVersion& version) {
		arcrop_require_api_version(version.Major, version.Minor);
	}

	inline void register_extension_callback(C2ResourceServiceNX::Hash40 hash, ExtensionCallbackFunction clbk) {
		arcrop_register_extension_callback(hash.as_u64(), clbk);
	}

	inline uint64_t get_decompressed_size(C2ResourceServiceNX::Hash40 hash) {
		uint64_t out_size = 0;
		arcrop_get_decompressed_size(hash.as_u64(), &out_size);
		return out_size;
	}

	inline void register_event_callback(Event e, EventCallbackFunction clbk) {
		arcrop_register_event_callback(e, clbk);
	}

	inline bool is_file_loaded(C2ResourceServiceNX::Hash40 hash) {
		return arcrop_is_file_loaded(hash.as_u64());
	}
}