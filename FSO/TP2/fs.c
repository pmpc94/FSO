/**
*	FSO - Trabalho prático nº 2
*
*	Francisco Godinho (nº 41611) e Pedro Carolina (nº 41665)
*	Turno prático 5 - Prof. Cecília Gomes
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#define FS_MAGIC		0xf0f03410
#define INODES_PER_BLOCK	128
#define POINTERS_PER_INODE	6
#define BLOCK_SIZE		4096
#define INODE_ENTRY_SIZE 	32
#define ALLOCATION_INODES	0.1


int *bitmap;				// pointer to the bitmap allocated in memory


struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int fs_format()
{
	union fs_block block;
	
	// checks if the disk is mounted
	if ( bitmap != NULL )
		return 0;

	// formatting superblock values
	block.super.magic = FS_MAGIC;
	block.super.nblocks = disk_size();
	block.super.ninodeblocks = ceil(ALLOCATION_INODES * block.super.nblocks);
	block.super.ninodes = block.super.ninodeblocks * (BLOCK_SIZE / INODE_ENTRY_SIZE);	
	disk_write(0, block.data);

	// marking all inodes as free
	int i, j, table_length = block.super.ninodeblocks;
	for ( i = 1; i <= table_length; i++ ) {
		for ( j = 0; j < INODES_PER_BLOCK; j++ )
			block.inode[j].isvalid = 0;
		disk_write(i, block.data);
	}
	return 1;
}

void fs_debug()
{
	union fs_block block;

	// debugging the superblock
	disk_read(0, block.data);

	// checks if magic number matches up
	if ( block.super.magic == FS_MAGIC ) {
		printf("superblock:\n");
		printf("    magic number is valid\n");
		printf("    %d blocks on disk\n", block.super.nblocks);
		printf("    %d blocks for inodes\n", block.super.ninodeblocks);
		printf("    %d inodes total\n", block.super.ninodes);

		// debugging the occupied blocks
		int i, j, k;
		int table_length = block.super.ninodeblocks;
		for ( i = 1; i <= table_length; i++ ) {
			disk_read(i, block.data);
			for ( j = 0; j < INODES_PER_BLOCK; j++ ) {
				if ( block.inode[j].isvalid ) {
					printf("inode %d:\n", j + (i-1)*INODES_PER_BLOCK);
					printf("    size %d bytes\n", block.inode[j].size);
					printf("    Blocks:");
					for ( k = 0; k < POINTERS_PER_INODE; k++ )
						if ( block.inode[j].direct[k] > 0 )
							printf(" %d", block.inode[j].direct[k]);
					printf("\n");
				}
			}
		}
	// magic number didn't match up
	} else
		 printf("invalid file system\n");
}

int fs_mount()
{
	union fs_block block;
	
	disk_read(0, block.data);
	// checks if magic number matches up and if disk isn't already mounted
	if ( block.super.magic != FS_MAGIC || bitmap !=  NULL )
		return 0;

	// builds a bitmap
	int bm[block.super.nblocks];

	// allocates some space for the bitmap
	int *pbm = bm;
	pbm = (int *) calloc(block.super.nblocks, sizeof(int));

	// fills bitmap with occupied blocks
	int i, j, k;	
	int table_length = block.super.ninodeblocks;
	for ( i = 0; i < table_length + 1; i++ )
		pbm[i] = 1;
	for ( i = 1; i <= table_length; i++ ) {
		disk_read(i, block.data);
		for ( j = 0; j < INODES_PER_BLOCK; j++ )
			if ( block.inode[j].isvalid )
				for ( k = 0; k < POINTERS_PER_INODE; k++ )
					if ( block.inode[j].direct[k] > 0 )
						pbm[block.inode[j].direct[k]] = 1;
	}
	// associates a global pointer with the bitmap for future use
	bitmap = pbm;
	return 1;
}

void reinitialize_inode( struct fs_inode *inode )
{
	// restarts the inode size to zero
	inode->size = 0;
	// clears inode adresses and bitmap positions so that the once occupied blocks can be free
	int i;
	for ( i = 0; i < POINTERS_PER_INODE; i++ ) {
		bitmap[inode->direct[i]] = 0;
		inode->direct[i] = 0;
	}
}

int fs_create()
{	
	union fs_block block;

	// checks if the disk is mounted
	if (bitmap == NULL)
		return -1;

	disk_read(0, block.data);
	int i, j;
	int found = 0;
	int table_length = block.super.ninodeblocks;

	// searching for a free entry in the inode table and clearing the block pointers
	for ( i = 1; i <= table_length && !found; i++ ) {
		disk_read(i, block.data);
		for ( j = 0; j < INODES_PER_BLOCK && !found; j++ ) {
			if ( !block.inode[j].isvalid ) {
				found = 1;
				block.inode[j].isvalid = 1;
				reinitialize_inode( &block.inode[j] );
			}
		}
	}

	// if the inode wasn't found, abort disk writing
	if (!found)
		return -1;

	disk_write(--i, block.data);
	return (i-1)*INODES_PER_BLOCK + j-1;
}

void inode_load( int inumber, struct fs_inode *inode ) {
	
	union fs_block block;
	// gets the specific inode block where the inode with the specified inumber might be located
	int table_block = 1 + (inumber / INODES_PER_BLOCK);
	// gets the inode's position on the respective inode block
	int inode_pos = inumber % INODES_PER_BLOCK;
	// reads the block and tries to obtain the inode by using the variables above
	disk_read(table_block, block.data);
	*inode = block.inode[inode_pos];
}

void inode_save( int inumber, struct fs_inode *inode ) {
	
	union fs_block block;
	// gets the specific inode block where the inode with the specified inumber might be located
	int table_block = 1 + (inumber / INODES_PER_BLOCK);
	// gets the inode's position on the respective inode block
	int inode_pos = inumber % INODES_PER_BLOCK;
	// tries to write the inode on the respective inode block using the variables above
	disk_read(table_block, block.data);
	block.inode[inode_pos] = *inode;
	disk_write(table_block, block.data);
}

int fs_delete( int inumber )
{
	struct fs_inode inode;
	
	// checks if the disk is mounted
	if ( bitmap == NULL )
		return -1;
	
	// loads the inode from disk
	inode_load(inumber, &inode);
	if ( !inode.isvalid )
		return -1;

	// reinitializes the inode adresses and size, frees the occupied data blocks in the bitmap
	reinitialize_inode(&inode);

	// marks inode as free and writes to disk
	inode.isvalid = 0;
	inode_save(inumber, &inode);
	return 0;
}

int fs_getsize( int inumber )
{
	struct fs_inode inode;

	// checks if the disk is mounted
	if (bitmap == NULL)
		return -1;

	// loads the inode from disk
	inode_load(inumber, &inode);
	if ( inode.isvalid )
		return inode.size;
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	struct fs_inode inode;
	union fs_block block;

	// loads the inode from disk
	inode_load(inumber, &inode);
	// checks if the disk is mounted and if the given offset is valid
	if ( bitmap == NULL || offset >= POINTERS_PER_INODE * BLOCK_SIZE || !inode.isvalid)
		return -1;
	// if every byte has already been written, abort disk reading
	if ( offset >= inode.size )
		return 0;

	// gets the inode's corresponding data block by the given offset
	int direct_pos = offset / BLOCK_SIZE;
	disk_read(inode.direct[direct_pos], block.data);

	// adjusts the position to start reading on the block
	int rdpos = offset % BLOCK_SIZE;
	int limit;
	if (BLOCK_SIZE < inode.size - offset)
		limit = BLOCK_SIZE;
	else if ( length < BLOCK_SIZE )	
		limit = length;
	else
		limit = inode.size - offset;
	// effectively writes the data block contents on the given array
	memmove(data, block.data + rdpos, limit);
	return limit;
}

int get_free_datablock()
{
	union fs_block block;
	disk_read(0, block.data);
	int free_pos, found;
	// searches for a free block in the bitmap
	for ( free_pos = block.super.ninodeblocks + 1, found = 0; free_pos < block.super.nblocks && !found; free_pos++ )
		if ( bitmap[free_pos] == 0 )
			found = 1;
	// if no free block was found, exit with an error
	if ( !found )
		return -1;

	// marks the previously found free block as occupied and returns its position on the bitmap
	bitmap[--free_pos] = 1;
	return free_pos;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	struct fs_inode inode;
	union fs_block block;

	// loads the inode, checks if the disk is mounted, if the given offset is valid and if the inode is valid
	inode_load(inumber, &inode);
	if ( bitmap == NULL || offset >= POINTERS_PER_INODE * BLOCK_SIZE || !inode.isvalid )
		return -1;
	// if the inode has some data blocks referenced and the writing has just started, reinitialize it
	if( inode.size && !offset ) 
		reinitialize_inode(&inode);

	// start byte by byte writing
	int direct_pos = offset / BLOCK_SIZE;
	int wrpos = offset % BLOCK_SIZE;
	int datapos;
	for ( datapos = 0; datapos < length; datapos++, wrpos++) {
		if ( wrpos == BLOCK_SIZE ) {
			wrpos = 0;
			// get a free data block and write on disk
			inode.direct[direct_pos] = get_free_datablock();
			disk_write(inode.direct[direct_pos++], block.data);
		}
		// write a byte to the block
		block.data[wrpos] = data[datapos];
	}
	// write to disk one last time
	inode.direct[direct_pos] = get_free_datablock();
	disk_write(inode.direct[direct_pos], block.data);
	// update inode size to the new size
	inode.size += length;
	inode_save(inumber, &inode);	
	return length;
}
