#include <stdio.h>
#include <fstream>

#include "fat32.h"

using std::ios;
using filesystem::fat32::FAT32;
using filesystem::fat32::sector_size;

uint8_t filebuf[sector_size] = {0};

class ScopedFile {
  public:
    ScopedFile(const char* filename) : file_(filename, ios::in | ios::binary) {}
    ~ScopedFile() { file_.close(); }

    std::fstream& file() { return file_; }

  private:
    std::fstream file_;
};

class DebugFAT32 : public FAT32 {
  public:
    DebugFAT32(uint8_t* first_sector) : FAT32(first_sector) {}

    void printStats() {
        printf("FAT32 Disk Statistics:\n");
        printf("----------------------\n");
        printf("# Bytes / Sector = %d\n", bytes_per_sector_);
        printf("Signature = 0x%04X\n", fs_signature_);
        printf("Partition Table Offset = %lu\n\n", partition_offset_);

        printf("# Reserved Sectors = %d\n", num_reserved_sectors_);
        printf("# of FATs = %d\n", num_FATs_);
        printf("Sectors / FAT = %d\n", sectors_per_FAT32_);
        printf("Root Directory Cluster# = %d\n", root_dir_cluster_);
        printf("Sectors / Cluster = %d\n", sectors_per_cluster_);
        printf("Root Sector = %d + (%d * %d) + ((%d - 2) * %d) = %d\n",
                num_reserved_sectors_,
                num_FATs_, sectors_per_FAT32_,
                root_dir_cluster_, sectors_per_cluster_,
                root_sector_);
        printf("Root Address = 0x%08lX\n\n", root_address());

        printf("volume_id = 0x%08x\n", volume_id_);
        printf("volume_label = ");
        for (int i = 0; i < sizeof(volume_label_); i++) {
            printf("%c", volume_label_[i]);
        }
        printf("\n");
        printf("fs_type = ");
        for (int i = 0; i < sizeof(fs_type_); i++) {
            printf("%c", fs_type_[i]);
        }
        printf("\n\n");

        if (isValid()) {
            printf("Found valid FAT32 disk.\n");
        } else {
            printf("FAT32 disk not valid!\n");
        }
        printf("----------------------\n\n");

    }
};

class DirectoryEntry {
  public:
    static constexpr size_t size = 32;

    typedef struct __attribute__((__packed__)) ShortNameDirectoryEntry {
        char     shortname[11];
        uint8_t  attributes;
        uint8_t  _unused1[8];
        uint16_t cluster_high;
        uint8_t  _unused2[4];
        uint16_t cluster_low;
        uint32_t size;
    } shortname_entry_t;

    typedef struct __attribute__((__packed__)) LongNameDirectoryEntry {
        uint8_t  sequence;
        char     longname1[10];
        uint8_t  attributes;
        uint8_t  zero1;
        uint8_t  checksum;
        char     longname2[12];
        uint16_t zero2;
        char     longname3[2];
    } longname_entry_t;

    typedef union GenericDirectoryEntry {
        uint8_t           raw[size];
        shortname_entry_t sne;
        longname_entry_t  lne;
    } entry_t;

    DirectoryEntry(uint8_t* buf) {
        entry_ = reinterpret_cast<entry_t*>(buf);
    }

    constexpr uint8_t attributes() const {
        return entry_->sne.attributes;
    }

    constexpr uint32_t filesize() const {
        return entry_->sne.size;
    }

    constexpr uint32_t folderCluster() const {
        return (entry_->sne.cluster_high << 16) | entry_->sne.cluster_low;
    }

    constexpr bool isFile() const {
        return (attributes() & 0x1E) == 0x00 && filesize() != 0;
    }

    constexpr bool isDirectory() const {
        return (attributes() & 0x10) == 0x10;
    }

    constexpr bool isLongFileName() const {
        return (attributes() & 0x0F) == 0x0F;
    }

    void appendName(char* namebuf) {
        if (namebuf == NULL) return;

        if (isLongFileName()) {
            longname_entry_t* lne = &entry_->lne;
            size_t sizeLFN = sizeof(lne->longname1)
                           + sizeof(lne->longname2)
                           + sizeof(lne->longname3);
            for (int i = 0; i < sizeLFN; i+=2) {
                if (i >= sizeof(lne->longname1) + sizeof(lne->longname2)) {
                    *namebuf = lne->longname3[i - sizeof(lne->longname1) - sizeof(lne->longname2)];
                } else if (i >= sizeof(lne->longname1)) {
                    *namebuf = lne->longname2[i - sizeof(lne->longname1)];
                } else {
                    *namebuf = lne->longname1[i];
                }
                if (*namebuf == '\0') return;
                namebuf++;
            }
        } else {
            shortname_entry_t* sne = &entry_->sne;
            for (int i = 0; i < sizeof(sne->shortname); i++) {
                *(namebuf++) = sne->shortname[i];
            }
        }
    }

  private:
    entry_t* entry_;
};

void printDirectoryEntry(DirectoryEntry* de) {
    char namebuf[32] = {0};
    if (de->isFile()) {
        printf("Found file: ");
        de->appendName(namebuf);
        printf("%s\n", namebuf);
    } else if (de->isLongFileName()) {
        printf("Found LFN: ");
        de->appendName(namebuf);
        printf("%s\n", namebuf);
    } else if (de->isDirectory()) {
        printf("Found dir: ");
        de->appendName(namebuf);
        printf("%s (cluster %d)\n", namebuf, de->folderCluster());
    }
}

int main(int argc, char** argv) {
    ScopedFile fatdisk("./floppy.img");

    fatdisk.file().seekg(0);
    fatdisk.file().read(reinterpret_cast<char*>(filebuf), sector_size);

    DebugFAT32 fat32disk(filebuf);
    fat32disk.printStats();

    fatdisk.file().seekg(fat32disk.root_address());
    fatdisk.file().read(reinterpret_cast<char*>(filebuf), sector_size);

    for (int i = 0; i < sector_size/DirectoryEntry::size; i++) {
        DirectoryEntry de(filebuf + i*DirectoryEntry::size);
        printDirectoryEntry(&de);
    }

    //uintptr_t nest_dir = getSubfolderAddress(root_dir_start, 1, 14);
    //printf("nest_dir = 0x%08lX\n", nest_dir);

    // TODO: get extension
    // TODO: assign callback type READ call to FAT class?
    // * internal sector storage
    // * can iterate directories by itself
    // TODO: edge cases
    // * filename too long for cluster?
    // * too many files for one cluster?
    // * file data > 1 cluster (MP3)

    return 0;
}

