// ext2 definitions from the real driver in the Linux kernel.
#include "ext2fs.h"

// This header allows your project to link against the reference library. If you
// complete the entire project, you should be able to remove this directive and
// still compile your code.

// Excluding the reference library header 
//#include "reference_implementation.h"

// Definitions for ext2cat to compile against.
#include "ext2_access.h"

///////////////////////////////////////////////////////////
//  Accessors for the basic components of ext2.
///////////////////////////////////////////////////////////

// Return a pointer to the primary superblock of a filesystem.
struct ext2_super_block * get_super_block(void * fs) {
  // superblock is always located at offset 1024 from the beginning of the file.    
  return fs + SUPERBLOCK_OFFSET; 
}


// Return the block size for a filesystem.
__u32 get_block_size(void * fs) {
  // Get the superblock of the file system.
  // And then get the superblock's size. 
  return EXT2_BLOCK_SIZE(get_super_block(fs));
}


// Return a pointer to a block given its number.
// get_block(fs, 0) == fs;
void * get_block(void * fs, __u32 block_num) {
  // offsets by the block size times the block_num. 
  return fs + (block_num * get_block_size(fs));
}


// Return a pointer to the first block group descriptor in a filesystem. Real
// ext2 filesystems will have several of these, but, for simplicity, we will
// assume there is only one.
struct ext2_group_desc * get_block_group(void * fs, __u32 block_group_num) {
  // Group descriptions are always right after the superblock.
  return (struct ext2_group_desc *)(fs + SUPERBLOCK_OFFSET + SUPERBLOCK_SIZE);
}


// Return a pointer to an inode given its number. In a real filesystem, this
// would require finding the correct block group, but you may assume it's in the
// first one.
struct ext2_inode * get_inode(void * fs, __u32 inode_num) {
  // We first need to get the inode table from block group descriptor.
  struct ext2_group_desc * desc = get_block_group(fs, 0);
  __u32 inode_table_block = desc->bg_inode_table; 
  struct ext2_inode * inode_table = get_block(fs, inode_table_block);
  // We then get the block from this table.
  // Offset this value by inode_num, and subtract 1 because inode 0 doesn't exist.
  return inode_table + inode_num - 1;
}



///////////////////////////////////////////////////////////
//  High-level code for accessing filesystem components by path.
///////////////////////////////////////////////////////////

// Chunk a filename into pieces.
// split_path("/a/b/c") will return {"a", "b", "c"}.
//
// This one's a freebie.
char ** split_path(char * path) {
    int num_slashes = 0;
    for (char * slash = path; slash != NULL; slash = strchr(slash + 1, '/')) {
        num_slashes++;
    }

    // Copy out each piece by advancing two pointers (piece_start and slash).
    char ** parts = (char **) calloc(num_slashes, sizeof(char *));
    char * piece_start = path + 1;
    int i = 0;
    for (char * slash = strchr(path + 1, '/');
         slash != NULL;
         slash = strchr(slash + 1, '/')) {
        int part_len = slash - piece_start;
        parts[i] = (char *) calloc(part_len + 1, sizeof(char));
        strncpy(parts[i], piece_start, part_len);
        piece_start = slash + 1;
        i++;
    }
    // Get the last piece.
    parts[i] = (char *) calloc(strlen(piece_start) + 1, sizeof(char));
    strncpy(parts[i], piece_start, strlen(piece_start));
    return parts;
}


// Convenience function to get the inode of the root directory.
struct ext2_inode * get_root_dir(void * fs) {
    return get_inode(fs, EXT2_ROOT_INO);
}


// Given the inode for a directory and a filename, return the inode number of
// that file inside that directory, or 0 if it doesn't exist there.
// 
// name should be a single component: "foo.txt", not "/files/foo.txt".
__u32 get_inode_from_dir(void * fs, struct ext2_inode * dir, 
        char * name) {
  // Number of inodes per block group is written in the superblock
  int num_inode = get_super_block(fs)->s_inodes_per_group;
  // Entry pointer to the inode table.
  struct ext2_dir_entry * entry = (struct ext2_dir_entry*)(get_block(fs, dir->i_block[0])); 

  // counter variable used for iteration
  int counter = 0;
  while(counter < num_inode) {
    // use the rec_len to go to next directory entry from the current entry
    //entry = (struct ext2_dir_entry*)(((void*)entry) + (counter * (int)entry->rec_len));

    // skip unused entries.
    if (entry->inode == 0) {
      continue;
    }

    // Check the name of the entry and the length of the name at the same time. 
    if (strlen(name) == (unsigned char)(entry->name_len) && strncmp(name, entry->name, strlen(name)) == 0) {
      // If equal, return the inode in the entry.
      return entry->inode;
    }
    // Otherwise, move to the next entry.
    entry = (struct ext2_dir_entry*)(((void*)entry) + entry->rec_len);
    // Increment the counter.
    counter++;
  }
  // couldn't find the inode. Return 0
  return 0;
}


// Find the inode number for a file by its full path.
// This is the functionality that ext2cat ultimately needs.
__u32 get_inode_by_path(void * fs, char * path) {
  // Use the split path to split the path name into directories.
  char ** paths = split_path(path);
  // Initialize the inode number as the root inode first.
  __u32 inode_num = EXT2_ROOT_INO;
  // Iterate through all the sub paths of the full path. 
  // Ex. For /code/python/ouroboros.py, iterate through /, /code, /code/python, /code/python/ouroboros.py
  for (char ** path = paths; *path != NULL; path++) {
    // Get the inode of the path
    struct ext2_inode * inode = get_inode(fs, inode_num);
    // If this is not a directory, then break.
    if(!LINUX_S_ISDIR(inode->i_mode)) break;
    // Get the inode num from the specified path.
    inode_num = get_inode_from_dir(fs, inode, *path);
    if(inode_num == 0) {
      break;
    } 
  }
  // If inode_num doesn't change, this means that there was no inode found.
  if (inode_num == EXT2_ROOT_INO) {
  // So return 0 here.
    return 0;
  } else {
    return inode_num;
  }
}

