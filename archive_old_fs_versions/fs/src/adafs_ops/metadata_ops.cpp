//
// Created by draze on 3/5/17.
//

#include "metadata_ops.h"
#include "dentry_ops.h"

// TODO error handling. Each read_metadata_field should check for boolean, i.e., if I/O failed.
bool write_all_metadata(const Metadata& md, const unsigned long hash) {
    write_metadata_field(md.atime(), hash, md_field_map.at(Md_fields::atime));
    write_metadata_field(md.mtime(), hash, md_field_map.at(Md_fields::mtime));
    write_metadata_field(md.ctime(), hash, md_field_map.at(Md_fields::ctime));
    write_metadata_field(md.uid(), hash, md_field_map.at(Md_fields::uid));
    write_metadata_field(md.gid(), hash, md_field_map.at(Md_fields::gid));
    write_metadata_field(md.mode(), hash, md_field_map.at(Md_fields::mode));
    write_metadata_field(md.inode_no(), hash, md_field_map.at(Md_fields::inode_no));
    write_metadata_field(md.link_count(), hash, md_field_map.at(Md_fields::link_count));
    write_metadata_field(md.size(), hash, md_field_map.at(Md_fields::size));
    write_metadata_field(md.blocks(), hash, md_field_map.at(Md_fields::blocks));

    return true;
}

// TODO error handling. Each read_metadata_field should check for nullptr, i.e., if I/O failed.
bool read_all_metadata(Metadata& md, const unsigned long hash) {
    md.atime(*read_metadata_field<time_t>(hash, md_field_map.at(Md_fields::atime)));
    md.mtime(*read_metadata_field<time_t>(hash, md_field_map.at(Md_fields::mtime)));
    md.ctime(*read_metadata_field<time_t>(hash, md_field_map.at(Md_fields::ctime)));
    md.uid(*read_metadata_field<uint32_t>(hash, md_field_map.at(Md_fields::uid)));
    md.gid(*read_metadata_field<uint32_t>(hash, md_field_map.at(Md_fields::gid)));
    md.mode(*read_metadata_field<uint32_t>(hash, md_field_map.at(Md_fields::mode)));
    md.inode_no(*read_metadata_field<uint64_t>(hash, md_field_map.at(Md_fields::inode_no)));
    md.link_count(*read_metadata_field<uint32_t>(hash, md_field_map.at(Md_fields::link_count)));
    md.size(*read_metadata_field<uint32_t>(hash, md_field_map.at(Md_fields::size)));
    md.blocks(*read_metadata_field<uint32_t>(hash, md_field_map.at(Md_fields::blocks)));
    return true;
}


int get_metadata(Metadata& md, const string& path) {
    return get_metadata(md, bfs::path(path));
}

int get_metadata(Metadata& md, const bfs::path& path) {
    ADAFS_DATA->logger->debug("get_metadata() enter for path {}", path.string());
    // Verify that the file is a valid dentry of the parent dir
    if (verify_dentry(path)) {
        // Metadata for file exists
        read_all_metadata(md, ADAFS_DATA->hashf(path.string()));
        return 0;
    } else {
        return -ENOENT;
    }
}

/**
 * Returns the metadata of an object based on its hash
 * @param path
 * @return
 */
// XXX Errorhandling
int remove_metadata(const unsigned long hash) {
    auto i_path = bfs::path(ADAFS_DATA->inode_path);
    i_path /= to_string(hash);
    // XXX below could be omitted
    if (!bfs::exists(i_path)) {
        ADAFS_DATA->logger->error("remove_metadata() metadata_path '{}' not found", i_path.string());
        return -ENOENT;
    }

    bfs::remove_all(i_path);
    // XXX make sure metadata has been deleted

    return 0;
}





