#ifndef __FILESYSTEM_FAT32_H
#define __FILESYSTEM_FAT32_H

namespace filesystem {
namespace fat32 {

static constexpr size_t sector_size = 512;

// https://en.wikipedia.org/wiki/BIOS_parameter_block
// https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
typedef struct __attribute__((__packed__)) ExtendedBiosParameterBlock {
    /* Common to FAT16/FAT32 */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;           // MBR offset 0x0D
    uint16_t num_reserved_sectors;          // MBR offset 0x0E
    uint8_t  num_FATs;                      // MBR offset 0x10
    uint16_t _unused_num_root_entries;
    uint16_t _unused_num_sectors_short;
    uint8_t  media_type;
    uint16_t sectors_per_FAT16;
    uint16_t _unused_sectors_per_track;
    uint16_t _unused_num_heads;
    uint32_t _unused_num_hidden_sectors;
    uint32_t _unused_num_sectors_long;
    /* FAT32 only */
    uint32_t sectors_per_FAT32;             // MBR offset 0x24
    uint16_t _unused_drive_descriptor;
    uint16_t _unused_version;
    uint32_t root_dir_cluster;              // MBR offset 0x2C
    uint16_t _unused_fs_info_sector_num;
    uint16_t _unused_boot_copy_sector_num;
    uint8_t  _unused_reserved_zero[12];
    uint8_t  _unused_drive_num;
    uint8_t  _unused_flags;
    uint8_t  _unused_extended_signature;    // == 0x29
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} ebpb_t;

typedef struct __attribute__((__packed__)) MBRPartitionEntry {
    uint8_t  boot_flag;
    uint8_t  CHS_begin[3];
    uint8_t  type_code;
    uint8_t  CHS_end[3];
    uint32_t LBA_begin;
    uint32_t num_sectors;
} mbrpe_t;

typedef struct __attribute__((__packed__)) MasterBootRecord {
    uint8_t  dummy_jmp[3];
    char     oem_name[8];
    ebpb_t   ebpb;
    uint8_t  _unused_boot_code[356];
    mbrpe_t  partition_entry[4];
    uint16_t fs_signature;                  // = 0xAA55
} mbr_t;

class FAT32 {
  public:
    FAT32(uint8_t* first_sector) {
        mbr_t* mbr = reinterpret_cast<mbr_t*>(first_sector);

        // cache traversal relevant values
        sectors_per_cluster_ = mbr->ebpb.sectors_per_cluster;
        num_reserved_sectors_ = mbr->ebpb.num_reserved_sectors;
        num_FATs_ = mbr->ebpb.num_FATs;
        sectors_per_FAT32_ = mbr->ebpb.sectors_per_FAT32;
        root_dir_cluster_ = mbr->ebpb.root_dir_cluster;

        // NOTE: units in sectors
        uint32_t partition_offset_from_reserved = mbr->partition_entry[0].LBA_begin;
        uint32_t FAT_sectors_start = num_reserved_sectors_ + partition_offset_from_reserved;
        uint32_t total_num_FAT_sectors = num_FATs_ * sectors_per_FAT32_;
        uint32_t data_sectors_start = FAT_sectors_start + total_num_FAT_sectors;
        uint32_t root_offset_from_data = start_sector_for_cluster(mbr->ebpb.root_dir_cluster);

        // save root
        root_sector_ = data_sectors_start + root_offset_from_data;

        // cache debug values
        bytes_per_sector_ = mbr->ebpb.bytes_per_sector;
        volume_id_ = mbr->ebpb.volume_id;
        for (int i = 0; i < sizeof(mbr->ebpb.volume_label); i++)
            volume_label_[i] = mbr->ebpb.volume_label[i];
        for (int i = 0; i < sizeof(mbr->ebpb.fs_type); i++)
            fs_type_[i] = mbr->ebpb.fs_type[i];
        fs_signature_ = mbr->fs_signature;
        partition_offset_ = reinterpret_cast<uintptr_t>(mbr->partition_entry) - reinterpret_cast<uintptr_t>(mbr);
    }

    bool isValid() {
        return bytes_per_sector_ == sector_size &&
            fs_signature_ == 0xAA55 &&
            partition_offset_ == 446;
    }

    // NOTE: relative to root
    constexpr int start_sector_for_cluster(int cluster_num) const {
        // NOTE: clusters #0 & #1 are reserved
        return ((cluster_num - 2) * sectors_per_cluster_);
    }

    constexpr uint32_t root_sector() const { return root_sector_; }

    constexpr uintptr_t root_address() const { return root_sector() * sector_size; }

    constexpr uint32_t subfolder_sector(int cluster_num) const {
        return root_sector() + start_sector_for_cluster(cluster_num);
    }

    constexpr uintptr_t subfolder_address(int cluster_num) const {
        return subfolder_address(cluster_num) * sector_size;
    }

  protected:
    // filesystem traversal values
    uint8_t  sectors_per_cluster_;
    uint16_t num_reserved_sectors_;
    uint8_t  num_FATs_;
    uint32_t sectors_per_FAT32_;
    uint32_t root_dir_cluster_;
    uint32_t root_sector_;

    // debug values
    uint16_t bytes_per_sector_;
    uint32_t volume_id_;
    char     volume_label_[11];
    char     fs_type_[8];
    uint16_t fs_signature_;
    uintptr_t partition_offset_;
};

} // namespace fat32
} // namespace filesystem

#endif // __FILESYSTEM_FAT32_H
