#pragma once

#include <iostream>
#include <cstdint>
#include <string>
#include "crc32.h"

#pragma pack(push, 1)

namespace C2ResourceServiceNX {

	struct LoadedArc;

	template <typename T> struct ClangVector {
		T* start;
		T* end;
		T* eos;
		// boo scary no bounds checking
		constexpr T& operator [] (int64_t index) {
			if (index < 0)
				return *(end + index);
			else
				return *(start + index);
		}
		constexpr size_t size() {
			return (end - start);
		}
		constexpr size_t capacity() {
			return (eos - start);
		}
	};
	enum class LoadState : int8_t
	{
		SadMoment = -1,
		Unused,
		Unloaded,
		Unk,
		Loaded,
	};
	enum class LoadType : uint32_t
	{
		LoadFromFilePackage,
		StandaloneFile
	};
	struct LoadInfo {
		LoadType load_type;
		uint32_t file_path_index;
		uint32_t dir_info_index;
		uint32_t file_count;
	};
	struct ResListNode {
		ResListNode* next;
		ResListNode* prev;
		LoadInfo data;
	};
	struct ResList {
		size_t size;
		ResListNode* head;
		ResListNode* end;
	};
	struct LoadedFilePath { // Also known as "Table1Entry"
		uint32_t loaded_data_index;
		uint32_t is_loaded;
	};
	struct LoadedData { // Also known as "Table2Entry"
		uint8_t* data_ptr;
		uint32_t ref_count;
		bool is_used;
		union {
			struct {
				LoadState load_state;
				bool file_flags2;
				uint8_t flags;
			};
			uint32_t all_load_state : 24;
		};
		uint32_t version;
		uint8_t unk;
	};
	struct LoadedDirectory {
		uint32_t file_group_index;
		uint32_t ref_count;
		uint8_t flags;
		union {
			struct {
				LoadState load_state;
				uint8_t child_ref_count;
				uint8_t unk;
			};
			uint32_t all_load_state : 24;
		};
		uint32_t incoming_request_count;
		ClangVector<uint32_t> child_path_indices;
		ClangVector<LoadedDirectory*> child_folders;
		LoadedDirectory* redirection_directory;
	};
	static_assert(sizeof(LoadedDirectory) == 72);

	struct PathInformation {
		LoadedArc* arc;
		void* loaded_file_system_search;
	};
	struct FilesystemInfo {
		nn::os::MutexType* mutex;
		LoadedFilePath* loaded_filepaths;
		LoadedData* loaded_datas;
		uint32_t loaded_filepath_len;
		uint32_t loaded_data_len;
		uint32_t loaded_filepath_count;
		uint32_t loaded_data_count;
		ClangVector<uint32_t> loaded_filepath_list;
		LoadedDirectory* loaded_directories;
		uint32_t loaded_directory_len;
		uint32_t loaded_directory_count;
		ClangVector<uint32_t> unk2;
		uint8_t unk3;
		char padding[7];
		void* addr;
		PathInformation* path_info;
		uint32_t version;
	};
	struct Hash40 {
		u32 crc;
		u32 len;

		Hash40() : crc(0), len(0) {};

		Hash40(u32 _crc, u32 _len) : crc(_crc), len(_len) {};

		Hash40(const std::string& str) {
			crc = crc32(str.c_str(), str.size());
			len = (u32)str.size();
		}

		static Hash40 From(const std::string& str) {
			return { crc32(str.c_str(), str.size()), (u32)str.size() };
		}

		uint64_t as_u64() const {
			return *(u64*)this;
		}

		bool operator == (const Hash40& other) const {
			return other.crc == crc && other.len == len;
		}
	};
	struct HashToIndex {
		uint32_t hash;
		uint32_t length : 8;
		uint32_t index : 24;

		HashToIndex() : hash(0), length(0), index(0) {};

		HashToIndex(Hash40 _hash, uint32_t _index) {
			hash = _hash.crc;
			length = _hash.len;
			index = _index;
		}

		Hash40 GetHash() const {
			return { hash, length };
		}

		static HashToIndex From(Hash40 _hash, uint32_t _index) {
			return { _hash, _index };
		}

		void SetHash(Hash40 _hash) {
			hash = _hash.crc;
			length = _hash.len;
		}
	};
	struct FileSystemHeader {
		uint32_t table_filesize;
		uint32_t file_info_path_count;
		uint32_t file_info_index_count;
		uint32_t folder_count;

		uint32_t folder_offset_count_1;

		uint32_t hash_folder_count;
		uint32_t file_info_count;
		uint32_t file_info_sub_index_count;
		uint32_t file_data_count;

		uint32_t folder_offset_count_2;
		uint32_t file_data_count_2;
		uint32_t padding;

		uint32_t unk1_10; // always 0x10
		uint32_t unk2_10; // always 0x10

		uint8_t regional_count_1;
		uint8_t regional_count_2;
		uint16_t padding2;

		uint32_t version;
		uint32_t extra_folder;
		uint32_t extra_count;

		uint32_t unk[2];

		uint32_t extra_count_2;
		uint32_t extra_sub_count;
	};
	struct RegionalStruct {
		uint32_t unk;
		uint32_t unk1;
		uint32_t index;
	};
	struct StreamHeader {
		uint32_t quick_dir_count;
		uint32_t stream_hash_count;
		uint32_t stream_file_index_count;
		uint32_t stream_offset_entry_count;
	};
	struct QuickDir {
		uint32_t hash;
		uint32_t name_length : 8;
		uint32_t count : 24;
		uint32_t index;
	};
	struct StreamEntry {
		HashToIndex path;
		uint32_t flags;
	};
	struct FileInfoBucket {
		uint32_t start;
		uint32_t count;
	};
	struct FilePath {
		HashToIndex path;
		HashToIndex ext;
		HashToIndex parent;
		HashToIndex file_name;
	};
	struct FileInfoIndex {
		uint32_t dir_offset_index;
		uint32_t file_info_index;
	};
	struct DirInfoFlags {
		uint32_t unk1 : 24;
		uint32_t is_regional : 1;
		uint32_t is_localized : 1;
		uint32_t redirected : 1;
		uint32_t has_regional_symlinked : 1;
		uint32_t is_symlink : 1;
		uint32_t unused : 3;
	};
	struct DirInfo {
		HashToIndex path;
		Hash40 name;
		Hash40 parent;
		uint32_t extra_dis_re;
		uint32_t extra_dis_re_length;
		uint32_t file_info_start_index;
		uint32_t file_count;
		uint32_t child_dir_start_index;
		uint32_t child_dir_count;
		union {
			DirInfoFlags flags;
			uint32_t flags_int;
		};
	};
	struct StreamData {
		uint64_t size;
		uint64_t offset;
	};
	struct DirectoryOffset {
		uint64_t offset;
		uint32_t decomp_size;
		uint32_t size;
		uint32_t file_start_index;
		uint32_t file_count;
		uint32_t directory_index;
	};
	struct FileInfoFlags {
		uint32_t unused : 4;
		// is_regular_file
		uint32_t is_redirect : 1;
		uint32_t unused2 : 7;
		uint32_t is_graphics_file : 1;
		uint32_t padding3 : 2;
		uint32_t is_regional : 1;
		uint32_t is_localized : 1;
		uint32_t unused3 : 3;
		uint32_t is_shared : 1;
		uint32_t unknown3 : 1;
		uint32_t unused4 : 10;
	};
	struct FileInfo {
		uint32_t file_path_index;
		uint32_t file_info_indice_index;
		uint32_t info_to_data_index;
		union {
			FileInfoFlags flags;
			uint32_t flags_int;
		};
	};
	struct FileInfoToFileData {
		uint32_t folder_offset_index;
		uint32_t file_data_index;
		union {
			struct {
				uint32_t file_info_idx : 24;
				uint32_t load_type : 8;
			};
			uint32_t file_info_index_and_load_type;
		};
	};
	struct FileDataFlags {
		uint32_t regular_zstd : 1;
		uint32_t compressed : 1;
		uint32_t is_versioned_regional : 1;
		uint32_t is_versioned_localized : 1;
		uint32_t unk : 28;
	};
	struct FileData {
		uint32_t offset_in_folder;
		uint32_t comp_size;
		uint32_t decomp_size;
		union {
			FileDataFlags flags;
			uint32_t flags_int;
		};
	};
	struct VersionedFile {
		uint32_t hash;
		uint32_t length : 8;
		uint32_t version : 24;
		uint32_t file_info_index;
		uint32_t folder_offset_index;
		uint32_t file_info_indice_index;

		Hash40 as_hash40() {
			return Hash40{ hash, length };
		}
	};
	struct LoadedVersion {
		VersionedFile* versioned_files;
		FileInfoBucket* bucket;
		HashToIndex* file_hashes;
		FileInfo* file_infos;
		FileInfoToFileData* file_info_to_datas;
		FileData* file_datas;
	};
	struct LoadedArc {
		u64 magic;
		u64 stream_offset;
		u64 file_data_offset;
		u64 shared_file_data_offset;
		u64 file_system_offset;
		u64 file_system_search_offset;
		u64 patch_section_offset;
		FileSystemHeader* uncompressed_fs;
		FileSystemHeader* fs_header;
		RegionalStruct* region_entries;
		FileInfoBucket* file_info_buckets;
		HashToIndex* file_hash_to_path_index;
		FilePath* file_paths;
		FileInfoIndex* file_info_indices;
		HashToIndex* dir_hash_to_info_index;
		DirInfo* dir_infos;
		DirectoryOffset* folder_offsets;
		HashToIndex* folder_child_hashes;
		FileInfo* file_infos;
		FileInfoToFileData* file_info_to_datas;
		FileData* file_datas;
		void* unk_section;
		StreamHeader* stream_header;
		QuickDir* quick_dirs;
		HashToIndex* stream_hash_to_entries;
		StreamEntry* stream_entries;
		u32* stream_file_indices;
		StreamData* stream_datas;
		FileInfoBucket* extra_buckets;
		void* extra_entries;
		DirectoryOffset* extra_folder_offsets;
		ClangVector<LoadedVersion> loaded_versions;
		u32 version;
		u32 extra_count;
		void* loaded_file_system_search;
		void* loaded_patch_section;

		uint32_t get_file_path_index_from_hash(const Hash40& hash) {
			for (int x = 0; x < this->fs_header->file_info_path_count; x++) {
				if (this->file_hash_to_path_index[x].GetHash() == hash) {
					return this->file_hash_to_path_index[x].index;
				}
			}
		}

		uint32_t get_file_info_index_from_hash(const Hash40& hash) {
			auto file_info_index_index = this->file_paths[this->get_file_path_index_from_hash(hash)].path.index;
			auto file_info_index = this->file_info_indices[file_info_index_index].file_info_index;
			return file_info_index;
		}

		FileInfo& get_file_info_from_hash(const Hash40& hash) {
			return this->file_infos[get_file_info_index_from_hash(hash)];
		}

		FileData& get_file_data_from_file_info(FileInfo& file_info) {
			auto& file_info_to_data = file_info_to_datas[file_info.info_to_data_index];
			return file_datas[file_info_to_data.file_data_index];
		}
	};
};

#pragma pack(pop)