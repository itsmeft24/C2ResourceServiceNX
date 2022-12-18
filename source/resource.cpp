
#include <nn/os.hpp>
#include <filesystem>
#include <vector>

#include "ARCTypes.hpp"
#include <skyline/utils/cpputils.hpp>
#include <skyline/inlinehook/And64InlineHook.hpp>
#include <skyline/logger/TcpLogger.hpp>

namespace nu {
	struct File_NX {
		void* vtable;
		void* unk1;
		u32 unk2;
		u32 is_open;
		nn::fs::FileHandle* file_handle;
		u32 unk3;
		u64 position;
		u8 filename_fixedstring[516];
		u32 unk4;

		size_t Read(void* buffer, size_t size) {
			auto x = (size_t(*)(File_NX*,void*,size_t))(uint64_t(skyline::utils::getRegionAddress(skyline::utils::region::Text)) + 0x37c58c0);
			return x(this, buffer, size);
		}
	};
};

namespace C2ResourceServiceNX {

	enum class LoadingType : uint32_t {
		LoadFromFilePackage = 0,
		LoadFromFileGroup = 1,
		LoadFromRegionalFileGroup = 2,
		Impossible = 3,
		StandaloneFile = 4,
	};

	struct ResServiceNX {
		nn::os::MutexType* mutex; // std::recursive_mutex*
		nn::os::EventType* res_update_event;
		nn::os::EventType* unk1;
		nn::os::EventType* io_swap_event;
		void* unk2;
		nn::os::SemaphoreType* semaphore_1;
		nn::os::SemaphoreType* semaphore_2;
		nn::os::ThreadType* res_update_thread;
		nn::os::ThreadType* res_loading_thread;
		nn::os::ThreadType* res_inflate_thread;
		void* unk3;
		ResList resource_lists[5];
		FilesystemInfo* filesystem_info;
		uint32_t localization_index;
		uint32_t region_index;
		uint32_t loading_thread_state;

		// ARCropolis has this defined as a single uint16_t, named state.
		uint8_t some_sort_of_state;
		bool current_index_loaded_status;

		bool is_loader_thread_running;
		uint8_t unk5;
		char data_arc_string[256];
		void* unk6;
		nu::File_NX** data_arc_file;
		size_t buffer_size;
		uint8_t* buffer_arr[2];
		uint32_t buffer_arr_idx;
		uint32_t unk12;
		uint8_t* data_ptr;
		uint64_t off_into_read;
		uint32_t processing_file_idx_curr;
		uint32_t processing_file_idx_count;
		uint32_t processing_file_idx_start;
		LoadingType processing_type;
		uint32_t processing_dir_idx_start;
		uint32_t processing_dir_idx_single;
		uint32_t current_index;
		uint32_t current_dir_index;
		bool is_updated;
		/*
		undefined field62_0x249;
		undefined field63_0x24a;
		undefined field64_0x24b;
		undefined field65_0x24c;
		undefined field66_0x24d;
		undefined field67_0x24e;
		undefined field68_0x24f;
		*/
	};

	struct ZSTD_Buffer {
		uint8_t* buffer;
		size_t size;
		size_t position;
	};

	FilesystemInfo* filesystem_info() {
		return *(C2ResourceServiceNX::FilesystemInfo**)((uint64_t)skyline::utils::getRegionAddress(skyline::utils::region::Text) + 87232288);
	}

	LoadedArc* arc() {
		filesystem_info()->path_info->arc;
	}

	inline void update_filesystem(FilesystemInfo* fs_info) {
		auto x = (void(*)(FilesystemInfo*))(uint64_t(skyline::utils::getRegionAddress(skyline::utils::region::Text)) + 0x353e780);
		x(fs_info);
	}

	inline int get_redirected_dir_load_state_recursive(LoadedDirectory* dir) {
		auto x = (int(*)(LoadedDirectory*))(uint64_t(skyline::utils::getRegionAddress(skyline::utils::region::Text)) + 0x35420a0);
		return x(dir);
	}

	inline void* jemalloc(size_t align, size_t size) {
		auto x = (void* (*)(size_t,size_t))(uint64_t(skyline::utils::getRegionAddress(skyline::utils::region::Text)) + 0x392cc60);
		return x(align, size);
	}

	void ResLoadingThread(C2ResourceServiceNX::ResServiceNX* service) {
		if (!service->is_loader_thread_running) {
			while (!service->is_loader_thread_running) {
				if (service->loading_thread_state != 2 && service->res_update_event != nullptr) {
					nn::os::WaitEvent(service->res_update_event);
				}
				if (service->is_loader_thread_running) {
					return;
				}
				// lock the res service, so our reslists dont mutate while we collect reslist nodes
				nn::os::LockMutex(service->mutex);

				std::vector<LoadInfo> load_requests;

				for (int res_list_index = 0; res_list_index < 5; res_list_index++) {
					ResListNode* node = service->resource_lists[res_list_index].head;
					while (node != nullptr && node != service->resource_lists[res_list_index].end) {
						load_requests.push_back(node->data);
						node = node->next;
					}
				}

				// unlock the res service
				nn::os::UnlockMutex(service->mutex);

				service->loading_thread_state = 2;

				for (const LoadInfo& load_request : load_requests) {
					if (load_request.load_type == LoadType::StandaloneFile) {
						// Get the file path index.
						uint32_t file_path_index = load_request.file_path_index;
						// Bounds-check the loaded file path table.
						if (file_path_index < service->filesystem_info->loaded_filepath_len) {
							// Get the loaded file path, and check to see if its being used.
							LoadedFilePath* loaded_filepath = &service->filesystem_info->loaded_filepaths[file_path_index];
							if (loaded_filepath->is_loaded) {
								// Get the loaded data and ensure that it is also being used. If so, continue on.
								// If the LoadedData's data pointer is already populated, we don't want to do anything.
								LoadedData* loaded_data = &service->filesystem_info->loaded_datas[loaded_filepath->loaded_data_index];
								if (loaded_data->is_used && loaded_data->data_ptr == nullptr) {
									// Perform a standard arc file lookup.
									FilePath* file_path = &service->filesystem_info->path_info->arc->file_paths[file_path_index];
									FileInfoIndex* file_info_index = &service->filesystem_info->path_info->arc->file_info_indices[file_path->path.index];

									FileInfo* file_info = &service->filesystem_info->path_info->arc->file_infos[file_info_index->file_info_index];
									DirectoryOffset* dir_offset = &service->filesystem_info->path_info->arc->folder_offsets[file_info_index->dir_offset_index];

									uint32_t info_to_data_index = file_info->info_to_data_index;

									if (file_info->flags.is_localized) {
										info_to_data_index += service->localization_index;
									}
									if (file_info->flags.is_regional) {
										info_to_data_index += service->region_index;
									}

									FileInfoToFileData* file_info_to_data = &service->filesystem_info->path_info->arc->file_info_to_datas[info_to_data_index];
									FileData* file_data = &service->filesystem_info->path_info->arc->file_datas[file_info_to_data->file_data_index];

									// Calculate the offset of the file.
									uint64_t offset = dir_offset->offset + file_data->offset_in_folder << 2;

									// (just here for example, subject to change)
									bool is_arcropolis_file = false;// file_data.flags.unk == 0x40000000;

									// Set the offset into read to 0.
									service->off_into_read = 0;

									if (!service->is_loader_thread_running) {
										while (true) {
											nn::os::AcquireSemaphore(service->semaphore_2);

											if (is_arcropolis_file) {
												/*
												// (just here for example, subject to change)
												auto stream = ARCropolis::GetFile(file_path->hash);

												// Read a chunk into the res service buffer.
												if (service->off_into_read + service->buffer_size >= file_data->decomp_size) {
													stream.read(service->buffer_arr[service->buffer_arr_idx], file_data->decomp_size - service->off_into_read);
													service->off_into_read = file_data->decomp_size;
												}
												else {
													stream.read(service->buffer_arr[service->buffer_arr_idx], service->buffer_size);
													service->off_into_read += service->buffer_size;
												}
												*/
											}
											else {
												// Set the seek on the File_NX instance.
												(*(service->data_arc_file))->position = offset + service->off_into_read;

												// Read a chunk into the res service buffer.
												if (service->off_into_read + service->buffer_size >= file_data->comp_size) {
													(*(service->data_arc_file))->Read(service->buffer_arr[service->buffer_arr_idx], file_data->comp_size - service->off_into_read);
													service->off_into_read = file_data->comp_size;
												}
												else {
													(*(service->data_arc_file))->Read(service->buffer_arr[service->buffer_arr_idx], service->buffer_size);
													service->off_into_read += service->buffer_size;
												}
											}

											// Prepare for the io swap event.
											service->processing_file_idx_start = file_info_index->file_info_index & 0xffffff;
											service->processing_file_idx_curr = 0;
											service->processing_file_idx_count = 1;
											service->processing_type = LoadingType::StandaloneFile;
											service->processing_dir_idx_start = 0xffffff;
											service->processing_dir_idx_single = 0xffffff;
											service->current_dir_index = 0xffffff;

											// Signal the io swap event. Let ResInflateThread know its time to do its thing.
											if (service->io_swap_event != nullptr) {
												nn::os::SignalEvent(service->io_swap_event);
											}

											// Toggle which buffer to use in the next chunk read.
											service->buffer_arr_idx ^= 1;
										}
										// All chunks have been read. We're done here!
										service->current_index_loaded_status = true;
										loaded_data->all_load_state = 0xffffffff;
									}
								}
							}
						}
					}
					if (load_request.load_type == LoadType::LoadFromFilePackage) {
						uint32_t dir_info_index = load_request.dir_info_index;
						if (dir_info_index != 0xffffff) {
							if (dir_info_index < service->filesystem_info->loaded_directory_len) {
								nn::os::LockMutex(service->filesystem_info->mutex);
								LoadedDirectory* loaded_dir = &service->filesystem_info->loaded_directories[dir_info_index];
								nn::os::UnlockMutex(service->filesystem_info->mutex);
								if (loaded_dir->flags & 1 && loaded_dir->file_group_index != 0xFFFFFF) {
									if (get_redirected_dir_load_state_recursive(loaded_dir) == 1) {
										loaded_dir->load_state = LoadState::Unk;
									}
								}
							}
						}
						DirInfo& dir = service->filesystem_info->path_info->arc->dir_infos[dir_info_index];
						if (dir.flags_int << 3 & 0xB) {
						}
					}
				}
			}
		}

	}

	void ResInflateThread(ResServiceNX* service) {

		while (true) {
			if (service->is_loader_thread_running) {
				return;
			}
			if (service->io_swap_event != nullptr) { // inb4 you need to deref two more times before comparing against nullptr
				nn::os::WaitEvent(service->io_swap_event);
			}
			if (service->is_loader_thread_running) {
				return;
			}
			if (!service->is_updated && (service->filesystem_info->unk3 != 0)) {
				nn::os::LockMutex(service->mutex);
				update_filesystem(service->filesystem_info);
				nn::os::UnlockMutex(service->mutex);
			}
			service->is_updated = true;

			for (service->processing_file_idx_curr = 0; service->processing_file_idx_curr < service->processing_file_idx_count; service->processing_file_idx_curr++) {

				uint32_t current_file_info_index = service->processing_file_idx_curr;
				FileData* current_file_data = nullptr;
				uint64_t current_offset_in_folder = 0;

				LoadedData* current_loaded_data = nullptr;

				void* source = nullptr;
				void* destination = nullptr;

				if (service->is_loader_thread_running) {
					nn::os::ReleaseSemaphore(service->semaphore_1);
					nn::os::ReleaseSemaphore(service->semaphore_2);
					service->is_updated = false;
					break;
				}
				switch (service->processing_type) {
					case LoadingType::LoadFromFilePackage:
					{
						uint32_t info_to_data_index = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].info_to_data_index;
						FileInfoFlags file_info_flags = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].flags;
						uint32_t file_info_flags_int = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].flags_int;

						if (file_info_flags.is_localized) {
							info_to_data_index += service->localization_index;
						}
						if (file_info_flags.is_regional) {
							info_to_data_index += service->region_index;
						}

						FileInfoToFileData file_info_to_data = service->filesystem_info->path_info->arc->file_info_to_datas[info_to_data_index];

						bool valid = ((file_info_flags_int >> 0x14 & 1) == 0) ||
							((file_info_to_data.file_info_index_and_load_type >> 0x1b & 1) != 0) &&
							((file_info_flags_int & 0x1800) == 0);
						// this looks very wrong but whatever
						// if (!file_info_flags.unknown1 && file_info_to_data.load_type == 1 && !file_info_flags.is_regional && !file_info_flags.is_localized) {
						if (valid) {

							if (file_info_to_data.file_data_index == 0xFFFFFF) { // The OG game never does this but we do :)
								skyline::TcpLogger::SendRaw("[C2ResourceServiceNX::ResInflateThread] Invalid FileDataIndex!\n");
							}

							current_file_data = &service->filesystem_info->path_info->arc->file_datas[file_info_to_data.file_data_index];

							current_offset_in_folder = static_cast<uint64_t>(current_file_data->offset_in_folder) << 2;

							uint32_t loaded_data_index = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].file_info_indice_index;

							if (loaded_data_index != 0xFFFFFF && loaded_data_index < service->filesystem_info->loaded_data_count) {
								if (service->filesystem_info->loaded_datas[loaded_data_index].is_used) {
									current_loaded_data = &service->filesystem_info->loaded_datas[loaded_data_index];
								}
							}
						}
						break;
					}
					case LoadingType::LoadFromFileGroup: // 1, almost identical to above case, has more file_info_to_data flag checks
					{
						uint32_t info_to_data_index = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].info_to_data_index;
						FileInfoFlags file_info_flags = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].flags;
						uint32_t file_info_flags_int = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].flags_int;

						if (file_info_flags.is_localized) {
							info_to_data_index += service->localization_index;
						}
						if (file_info_flags.is_regional) {
							info_to_data_index += service->region_index;
						}

						FileInfoToFileData file_info_to_data = service->filesystem_info->path_info->arc->file_info_to_datas[info_to_data_index];

						bool valid = ((file_info_flags_int >> 0x14 & 1) == 0) ||
							((file_info_to_data.file_info_index_and_load_type >> 0x1b & 1) != 0);
						if (valid) {

							if (file_info_to_data.file_data_index == 0xFFFFFF) { // The OG game never does this but we do :)
								skyline::TcpLogger::SendRaw("[C2ResourceServiceNX::ResInflateThread] Invalid FileDataIndex!\n");
							}

							current_file_data = &service->filesystem_info->path_info->arc->file_datas[file_info_to_data.file_data_index];

							current_offset_in_folder = static_cast<uint64_t>(current_file_data->offset_in_folder) << 2;

							uint32_t loaded_data_index = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].file_info_indice_index;

							if (loaded_data_index != 0xFFFFFF && loaded_data_index < service->filesystem_info->loaded_data_count) {
								if (service->filesystem_info->loaded_datas[loaded_data_index].is_used) {
									current_loaded_data = &service->filesystem_info->loaded_datas[loaded_data_index];
								}
							}
						}
						break;
					}
					case LoadingType::LoadFromRegionalFileGroup: // almost identical to above case, has even more file_info_to_data flag checks
					{
						uint32_t info_to_data_index = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].info_to_data_index;
						FileInfoFlags file_info_flags = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].flags;
						uint32_t file_info_flags_int = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].flags_int;

						if (file_info_flags.is_localized) {
							info_to_data_index += service->localization_index;
						}
						if (file_info_flags.is_regional) {
							info_to_data_index += service->region_index;
						}

						FileInfoToFileData file_info_to_data = service->filesystem_info->path_info->arc->file_info_to_datas[info_to_data_index];

						bool valid = ((static_cast<unsigned int>((file_info_flags_int & 0x100000) == 0) | (file_info_to_data.file_info_index_and_load_type & 0x8000000) >> 0x1b) == 1 && (file_info_to_data.file_info_index_and_load_type & 0x10000000) == 0)
							&& (file_info_flags_int >> 0xf & 3) != 0;
						if (valid) {

							if (file_info_to_data.file_data_index == 0xFFFFFF) { // The OG game never does this but we do :)
								skyline::TcpLogger::SendRaw("[C2ResourceServiceNX::ResInflateThread] Invalid FileDataIndex!\n");
							}

							current_file_data = &service->filesystem_info->path_info->arc->file_datas[file_info_to_data.file_data_index];

							current_offset_in_folder = static_cast<uint64_t>(current_file_data->offset_in_folder) << 2;

							uint32_t loaded_data_index = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].file_info_indice_index;

							if (loaded_data_index != 0xFFFFFF && loaded_data_index < service->filesystem_info->loaded_data_count) {
								if (service->filesystem_info->loaded_datas[loaded_data_index].is_used) {
									current_loaded_data = &service->filesystem_info->loaded_datas[loaded_data_index];
								}
							}
						}
						break;
					}
					case LoadingType::Impossible: // literally never gets hit. og game doesnt even check for this one :dead:
						break;
					case LoadingType::StandaloneFile: // individual file. has checks for versioned file stuff. screw versioned files :)
					{	
						uint32_t info_to_data_index = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].info_to_data_index;
						FileInfoFlags file_info_flags = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].flags;

						if (file_info_flags.is_localized) {
							info_to_data_index += service->localization_index;
						}
						if (file_info_flags.is_regional) {
							info_to_data_index += service->region_index;
						}

						FileInfoToFileData file_info_to_data = service->filesystem_info->path_info->arc->file_info_to_datas[info_to_data_index];

						if (file_info_to_data.file_data_index == 0xFFFFFF) { // The OG game never does this but we do :)
							skyline::TcpLogger::SendRaw("[C2ResourceServiceNX::ResInflateThread] Invalid FileDataIndex!\n");
						}

						current_file_data = &service->filesystem_info->path_info->arc->file_datas[file_info_to_data.file_data_index];

						current_offset_in_folder = static_cast<uint64_t>(current_file_data->offset_in_folder) << 2;

						uint32_t loaded_data_index = service->filesystem_info->path_info->arc->file_infos[current_file_info_index].file_info_indice_index;

						if (loaded_data_index != 0xFFFFFF && loaded_data_index < service->filesystem_info->loaded_data_count) {
							if (service->filesystem_info->loaded_datas[loaded_data_index].is_used) {
								current_loaded_data = &service->filesystem_info->loaded_datas[loaded_data_index];
							}
						}
						break;
					}
				}

				bool does_not_require_load = false;

				// undefined behavior, more like "among us"
				if (current_loaded_data == nullptr || current_loaded_data->ref_count < 1 || current_loaded_data->load_state == LoadState::Unused) {
					does_not_require_load = true;
				}
				else {
					if (current_loaded_data->data_ptr != nullptr) {
						current_loaded_data->load_state = LoadState::Loaded;
						does_not_require_load = true;
					}
				}
				// If we need to load...
				if (!does_not_require_load) {

					if (service->off_into_read <= 0) {
						nn::os::ReleaseSemaphore(service->semaphore_1);
						nn::os::ReleaseSemaphore(service->semaphore_2);
						if (service->is_loader_thread_running) {
							service->is_updated = false;
							nn::os::ReleaseSemaphore(service->semaphore_1);
							nn::os::ReleaseSemaphore(service->semaphore_2);
							continue;
						}
						if (service->io_swap_event != nullptr) {
							nn::os::WaitEvent(service->io_swap_event);
						}
						source = service->data_ptr;
					}
					if (!current_loaded_data->file_flags2) {
						// allocate new data ptr, align according to fs_header
						destination = jemalloc(service->filesystem_info->path_info->arc->fs_header->unk1_10, current_file_data->decomp_size);
						current_loaded_data->data_ptr = reinterpret_cast<uint8_t*>(destination);
					}
					else {
						destination = current_loaded_data->data_ptr;
					}
					current_loaded_data->flags &= 0xbf;
					auto compression_type = current_file_data->flags_int & 0x7;
					switch (compression_type)
					{
					case 0:
					{
						if (service->off_into_read >= current_offset_in_folder + current_file_data->comp_size) {
							std::memcpy(destination, source, current_file_data->comp_size);
							break;
						}
						uint64_t bytes_read = service->off_into_read - current_offset_in_folder;
						std::memcpy(destination, source, bytes_read);
						if (bytes_read < current_file_data->comp_size) {
							auto* ptr = reinterpret_cast<uint8_t*>(destination) + bytes_read;
							do {
								auto uVar4 = bytes_read;
								nn::os::ReleaseSemaphore(service->semaphore_1);
								nn::os::ReleaseSemaphore(service->semaphore_2);
								if (service->is_loader_thread_running) {
									// service->is_updated = false;
									// nn::os::ReleaseSemaphore(service->semaphore_1);
									// nn::os::ReleaseSemaphore(service->semaphore_2);
									// continue;
									// next loop iter
								}
								if (service->io_swap_event != nullptr) {
									nn::os::WaitEvent(service->io_swap_event);
								}
								uVar36 -= uVar4;
								source = service->data_ptr;
								uVar42 -= bytes_read;
								current_offset_in_folder += bytes_read;
								bytes_read = uVar12;
								if (service->off_into_read < uVar12) {
									bytes_read = service->off_into_read;
								}
								bytes_read = bytes_read - uVar32;
								nn::os::YieldThread();
								std::memcpy(ptr, source, bytes_read);
								ptr+=bytes_read;
							} while (bytes_read < current_file_data->comp_size);
						}
						// uncompressed, just memcpy
						break;
					}
					case 1:
					{
						// std::abort(); we could prob use this for something :thonk:
						break;
					}
					case 2:
					{
						// unused/funky compression
						break;
					}
					case 3:
					{
						// ZSTD_decompressStream
						break;
					}
					}
				}
			}
			if (service->current_index != 0xFFFFFF) {
				if (service->current_index < service->filesystem_info->loaded_directory_len) {
					nn::os::LockMutex(service->mutex);
					LoadedDirectory* currently_loaded_directory = &service->filesystem_info->loaded_directories[service->current_index];
					nn::os::UnlockMutex(service->mutex);
					// todo: properly handle if currently_loaded_directory isnt a valid ptr anymore
					if (currently_loaded_directory->flags & 1 && currently_loaded_directory) {
						if (!service->current_index_loaded_status) {
							// CALL 7103541E30 (currently_loaded_directory);
						}
						else {
							currently_loaded_directory->load_state = LoadState::SadMoment;
						}
					}
				}
			}
			if (service->processing_dir_idx_single != 0xFFFFFF) {
				if (service->processing_dir_idx_single < service->filesystem_info->loaded_directory_len) {
					nn::os::LockMutex(service->mutex);
					LoadedDirectory* single_loaded_directory = &service->filesystem_info->loaded_directories[service->processing_dir_idx_single];
					nn::os::UnlockMutex(service->mutex);
					// todo: properly handle if single_loaded_directory isnt a valid ptr anymore
					if (single_loaded_directory->flags & 1 && single_loaded_directory) {
						if (!service->current_index_loaded_status) {
							if (single_loaded_directory->file_group_index != 0xFFFFFF && get_redirected_dir_load_state_recursive(single_loaded_directory) == 2) {
								// CALL 7103541E30 (single_loaded_directory);
							}
						}
						else {
							single_loaded_directory->load_state = LoadState::SadMoment;
						}
					}
				}
			}
			if (!service->current_index_loaded_status && service->processing_type == LoadingType::LoadFromRegionalFileGroup) {
				if (service->processing_dir_idx_start != 0xFFFFFF && service->processing_dir_idx_start < service->filesystem_info->loaded_directory_len) {
					nn::os::LockMutex(service->mutex);
					LoadedDirectory* processing_loaded_directory = &service->filesystem_info->loaded_directories[service->current_index];
					nn::os::UnlockMutex(service->mutex);
					// todo: properly handle if processing_loaded_directory isnt a valid ptr anymore
					if (processing_loaded_directory->flags & 1 && processing_loaded_directory) {
						if (service->processing_type == LoadingType::LoadFromFilePackage) {
							processing_loaded_directory->flags |= 2;
						}
						else {
							processing_loaded_directory->flags |= 4;
						}
					}
				}
			}
			service->is_updated = false;
			nn::os::ReleaseSemaphore(service->semaphore_1);
			nn::os::ReleaseSemaphore(service->semaphore_2);
		}
	};

	void ResUpdateThread(ResServiceNX* service) {
		while (!service->is_loader_thread_running) {
			if (!service->is_updated && service->filesystem_info->unk3 != 0) {
				nn::os::LockMutex(service->mutex);
				update_filesystem(service->filesystem_info);
				nn::os::UnlockMutex(service->mutex);
			}
			if (service->some_sort_of_state > 0) {
				if (service->some_sort_of_state < 3) {
					service->some_sort_of_state++;
				}
				else if (service->some_sort_of_state == 3 && service->loading_thread_state != 2) {
					nn::os::LockMutex(service->mutex);
					service->loading_thread_state = 2;
					nn::os::UnlockMutex(service->mutex);
					if (service->res_update_event != nullptr) {
						nn::os::SignalEvent(service->res_update_event);
					}
					service->some_sort_of_state = 4;
				}
			}
			nn::os::SleepThread(nn::TimeSpan::FromNanoSeconds(16000000));
		}
		return;
	}

};


void InitEx() {
	auto inflate = (void(*)(C2ResourceServiceNX::ResServiceNX*))(uint64_t(skyline::utils::getRegionAddress(skyline::utils::region::Text)) + 0x3543a90);
	auto loading = (void(*)(C2ResourceServiceNX::ResServiceNX*))(uint64_t(skyline::utils::getRegionAddress(skyline::utils::region::Text)) + 0x3542660);
	auto update = (void(*)(C2ResourceServiceNX::ResServiceNX*))(uint64_t(skyline::utils::getRegionAddress(skyline::utils::region::Text)) + 0x3542570);
	void* inflate_original = nullptr;
	void* loading_original = nullptr;
	void* update_original = nullptr;
	A64HookFunction(inflate, &C2ResourceServiceNX::ResInflateThread, &inflate_original);
	A64HookFunction(loading, &C2ResourceServiceNX::ResLoadingThread, &loading_original);
	A64HookFunction(update, &C2ResourceServiceNX::ResUpdateThread, &update_original);
}
